// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboInternal.h"
#include "UeaboHygiene.h"
#include "UeaboNodeMetrics.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "Algo/Reverse.h"

namespace
{
	using namespace UeaboInternal;

	struct FRect
	{
		float X = 0.f, Y = 0.f, W = 0.f, H = 0.f;
		float Right() const { return X + W; }
		float Bottom() const { return Y + H; }
		bool Overlaps(const FRect& O) const { return X < O.Right() && O.X < Right() && Y < O.Bottom() && O.Y < Bottom(); }
		bool Contains(float PX, float PY) const { return PX >= X && PX <= Right() && PY >= Y && PY <= Bottom(); }
	};

	struct FNodeInfo
	{
		UEdGraphNode* Node = nullptr;
		float W = 0.f, H = 0.f;
		float X = 0.f, Y = 0.f;
		float OrigX = 0.f, OrigY = 0.f;
		bool bExec = false;
		int32 Lane = -1;
		int32 Rank = 0;
		int32 Order = 0;
		int32 Depth = 0;          // data column depth (1 = next to its consumer); 0 for chain nodes
		int32 Anchor = -1;        // data nodes: the consumer they are aligned to
		UEdGraphPin* AnchorPin = nullptr; // consumer input pin
		UEdGraphPin* MyPin = nullptr;     // own output pin feeding AnchorPin
		TArray<int32> Succ;       // chain edges inside the lane (back edges removed)
		TArray<int32> Pred;
		FRect Rect() const { FRect R; R.X = X; R.Y = Y; R.W = W; R.H = H; return R; }
	};

	struct FLane
	{
		TArray<int32> Roots;
		TArray<int32> Chain;      // exec (or data-lane) members
		TArray<int32> Data;       // data nodes placed in this lane's gutters
		bool bDataLane = false;
		int32 MaxRank = 0;
	};

	struct FLayoutContext
	{
		UEdGraph* Graph = nullptr;
		const FUeaboOrganizeOptions* Options = nullptr;
		TArray<FNodeInfo> Infos;
		TMap<UEdGraphNode*, int32> Index;
		TArray<FLane> Lanes;

		int32 Find(UEdGraphNode* Node) const
		{
			const int32* Found = Index.Find(Node);
			return Found ? *Found : INDEX_NONE;
		}
	};

	bool EarlierInGraph(const FNodeInfo& A, const FNodeInfo& B)
	{
		if (A.OrigY != B.OrigY)
		{
			return A.OrigY < B.OrigY;
		}
		return A.OrigX < B.OrigX;
	}

	/** Chain successors of a node: exec links for exec lanes, data links for the data lane. */
	void ChainSuccessors(const FLayoutContext& Ctx, int32 I, bool bDataLane, TArray<int32>& Out)
	{
		for (UEdGraphPin* Pin : Ctx.Infos[I].Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin) == bDataLane)
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				const int32 J = Linked ? Ctx.Find(Linked->GetOwningNode()) : INDEX_NONE;
				if (J != INDEX_NONE && J != I)
				{
					Out.AddUnique(J);
				}
			}
		}
	}

	/** Assigns lanes to a set of chain nodes: BFS from the roots, then islands. */
	void BuildLanes(FLayoutContext& Ctx, const TArray<int32>& Members, bool bDataLane)
	{
		TSet<int32> MemberSet(Members);
		TMap<int32, TArray<int32>> Preds;
		for (int32 I : Members)
		{
			TArray<int32> Succ;
			ChainSuccessors(Ctx, I, bDataLane, Succ);
			for (int32 J : Succ)
			{
				if (MemberSet.Contains(J))
				{
					Preds.FindOrAdd(J).Add(I);
				}
			}
		}

		auto Flood = [&](FLane& Lane, int32 Root)
		{
			TArray<int32> Queue;
			Queue.Add(Root);
			Ctx.Infos[Root].Lane = Ctx.Lanes.Num() - 1;
			Lane.Chain.Add(Root);
			for (int32 Q = 0; Q < Queue.Num(); ++Q)
			{
				TArray<int32> Succ;
				ChainSuccessors(Ctx, Queue[Q], bDataLane, Succ);
				for (int32 J : Succ)
				{
					if (MemberSet.Contains(J) && Ctx.Infos[J].Lane == -1)
					{
						Ctx.Infos[J].Lane = Ctx.Lanes.Num() - 1;
						Lane.Chain.Add(J);
						Queue.Add(J);
					}
				}
			}
		};

		TArray<int32> Sorted = Members;
		Sorted.Sort([&](int32 A, int32 B) { return EarlierInGraph(Ctx.Infos[A], Ctx.Infos[B]); });

		if (bDataLane)
		{
			// One lane holding every loose data island; roots are the nodes without a loose producer.
			Ctx.Lanes.AddDefaulted();
			FLane& Lane = Ctx.Lanes.Last();
			Lane.bDataLane = true;
			for (int32 I : Sorted)
			{
				if (!Preds.Contains(I))
				{
					Lane.Roots.Add(I);
				}
			}
			for (int32 Root : Lane.Roots)
			{
				if (Ctx.Infos[Root].Lane == -1)
				{
					TArray<int32> Queue;
					Queue.Add(Root);
					Ctx.Infos[Root].Lane = Ctx.Lanes.Num() - 1;
					Lane.Chain.Add(Root);
					for (int32 Q = 0; Q < Queue.Num(); ++Q)
					{
						TArray<int32> Succ;
						ChainSuccessors(Ctx, Queue[Q], true, Succ);
						for (int32 J : Succ)
						{
							if (MemberSet.Contains(J) && Ctx.Infos[J].Lane == -1)
							{
								Ctx.Infos[J].Lane = Ctx.Lanes.Num() - 1;
								Lane.Chain.Add(J);
								Queue.Add(J);
							}
						}
					}
				}
			}
			for (int32 I : Sorted)
			{
				if (Ctx.Infos[I].Lane == -1)
				{
					// Pure cycle without a root: start it at its earliest node.
					Lane.Roots.Add(I);
					Ctx.Infos[I].Lane = Ctx.Lanes.Num() - 1;
					Lane.Chain.Add(I);
				}
			}
			return;
		}

		for (int32 I : Sorted)
		{
			if (IsRootNode(Ctx.Infos[I].Node))
			{
				Ctx.Lanes.AddDefaulted();
				FLane& Lane = Ctx.Lanes.Last();
				Lane.Roots.Add(I);
				Flood(Lane, I);
			}
		}
		// Islands: exec nodes no root reaches. Each island starts at its earliest node without an unassigned predecessor.
		for (;;)
		{
			int32 Start = INDEX_NONE;
			for (int32 I : Sorted)
			{
				if (Ctx.Infos[I].Lane != -1)
				{
					continue;
				}
				bool bHasOpenPred = false;
				if (const TArray<int32>* P = Preds.Find(I))
				{
					for (int32 J : *P)
					{
						bHasOpenPred |= Ctx.Infos[J].Lane == -1;
					}
				}
				if (!bHasOpenPred)
				{
					Start = I;
					break;
				}
			}
			if (Start == INDEX_NONE)
			{
				for (int32 I : Sorted)
				{
					if (Ctx.Infos[I].Lane == -1)
					{
						Start = I;
						break;
					}
				}
			}
			if (Start == INDEX_NONE)
			{
				break;
			}
			Ctx.Lanes.AddDefaulted();
			FLane& Lane = Ctx.Lanes.Last();
			Lane.Roots.Add(Start);
			Flood(Lane, Start);
		}
	}

	/** Removes back edges (DFS) and computes longest-path ranks inside each lane. */
	void RankLanes(FLayoutContext& Ctx, int32 FirstLane)
	{
		for (int32 L = FirstLane; L < Ctx.Lanes.Num(); ++L)
		{
			FLane& Lane = Ctx.Lanes[L];
			TMap<int32, int32> State; // 0 unseen, 1 on stack, 2 done
			int32 Preorder = 0;
			TArray<int32> Topo; // reverse postorder

			TFunction<void(int32)> Visit = [&](int32 I)
			{
				State.Add(I, 1);
				Ctx.Infos[I].Order = Preorder++;
				TArray<int32> Succ;
				ChainSuccessors(Ctx, I, Lane.bDataLane, Succ);
				for (int32 J : Succ)
				{
					if (Ctx.Infos[J].Lane != L)
					{
						continue;
					}
					const int32 S = State.FindRef(J);
					if (S == 1)
					{
						continue; // back edge
					}
					Ctx.Infos[I].Succ.AddUnique(J);
					Ctx.Infos[J].Pred.AddUnique(I);
					if (S == 0)
					{
						Visit(J);
					}
				}
				State.Add(I, 2);
				Topo.Add(I);
			};
			for (int32 Root : Lane.Roots)
			{
				if (!State.Contains(Root))
				{
					Visit(Root);
				}
			}
			for (int32 I : Lane.Chain)
			{
				if (!State.Contains(I))
				{
					Visit(I);
				}
			}
			Algo::Reverse(Topo);
			for (int32 I : Topo)
			{
				Ctx.Infos[I].Rank = 0;
			}
			for (int32 I : Topo)
			{
				for (int32 J : Ctx.Infos[I].Succ)
				{
					Ctx.Infos[J].Rank = FMath::Max(Ctx.Infos[J].Rank, Ctx.Infos[I].Rank + 1);
				}
			}
			for (int32 I : Lane.Chain)
			{
				Lane.MaxRank = FMath::Max(Lane.MaxRank, Ctx.Infos[I].Rank);
			}
		}
	}

	/** Data nodes go into the gutter left of their first (leftmost) consumer. */
	void AssignDataNodes(FLayoutContext& Ctx)
	{
		TFunction<void(int32, int32)> Feed = [&](int32 Consumer, int32 LaneIdx)
		{
			for (UEdGraphPin* Pin : Ctx.Infos[Consumer].Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Input || IsExecPin(Pin))
				{
					continue;
				}
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					const int32 J = Linked ? Ctx.Find(Linked->GetOwningNode()) : INDEX_NONE;
					if (J == INDEX_NONE || Ctx.Infos[J].bExec || Ctx.Infos[J].Lane != -1)
					{
						continue;
					}
					FNodeInfo& D = Ctx.Infos[J];
					D.Lane = LaneIdx;
					D.Rank = Ctx.Infos[Consumer].Rank;
					D.Depth = Ctx.Infos[Consumer].Depth + 1;
					D.Anchor = Consumer;
					D.AnchorPin = Pin;
					D.MyPin = Linked;
					Ctx.Lanes[LaneIdx].Data.Add(J);
					Feed(J, LaneIdx);
				}
			}
		};
		for (int32 L = 0; L < Ctx.Lanes.Num(); ++L)
		{
			TArray<int32> Chain = Ctx.Lanes[L].Chain;
			Chain.Sort([&](int32 A, int32 B)
			{
				const FNodeInfo& IA = Ctx.Infos[A];
				const FNodeInfo& IB = Ctx.Infos[B];
				return IA.Rank != IB.Rank ? IA.Rank < IB.Rank : IA.Order < IB.Order;
			});
			for (int32 I : Chain)
			{
				Feed(I, L);
			}
		}
	}

	int32 CountLayerCrossings(const FLayoutContext& Ctx, const TArray<TArray<int32>>& Layers)
	{
		TMap<int32, int32> Pos;
		for (const TArray<int32>& Layer : Layers)
		{
			for (int32 K = 0; K < Layer.Num(); ++K)
			{
				Pos.Add(Layer[K], K);
			}
		}
		int32 Crossings = 0;
		for (int32 R = 0; R + 1 < Layers.Num(); ++R)
		{
			TArray<TPair<int32, int32>> Edges;
			for (int32 I : Layers[R])
			{
				for (int32 J : Ctx.Infos[I].Succ)
				{
					if (Ctx.Infos[J].Rank == R + 1)
					{
						Edges.Add(TPair<int32, int32>(Pos[I], Pos[J]));
					}
				}
			}
			for (int32 A = 0; A < Edges.Num(); ++A)
			{
				for (int32 B = A + 1; B < Edges.Num(); ++B)
				{
					if ((Edges[A].Key - Edges[B].Key) * (Edges[A].Value - Edges[B].Value) < 0)
					{
						++Crossings;
					}
				}
			}
		}
		return Crossings;
	}

	/** Barycenter ordering inside each rank: 4 down/up sweeps, best crossing count kept. */
	TArray<TArray<int32>> OrderLane(FLayoutContext& Ctx, const FLane& Lane)
	{
		TArray<TArray<int32>> Layers;
		Layers.SetNum(Lane.MaxRank + 1);
		TArray<int32> Chain = Lane.Chain;
		Chain.Sort([&](int32 A, int32 B) { return Ctx.Infos[A].Order < Ctx.Infos[B].Order; });
		for (int32 I : Chain)
		{
			Layers[Ctx.Infos[I].Rank].Add(I);
		}

		TArray<TArray<int32>> Best = Layers;
		int32 BestCrossings = CountLayerCrossings(Ctx, Layers);

		auto SortByBarycenter = [&](TArray<int32>& Layer, const TArray<int32>& Neighbor, bool bUsePred)
		{
			TMap<int32, int32> NPos;
			for (int32 K = 0; K < Neighbor.Num(); ++K)
			{
				NPos.Add(Neighbor[K], K);
			}
			TMap<int32, float> Key;
			for (int32 K = 0; K < Layer.Num(); ++K)
			{
				const FNodeInfo& Info = Ctx.Infos[Layer[K]];
				const TArray<int32>& Links = bUsePred ? Info.Pred : Info.Succ;
				float Sum = 0.f;
				int32 Count = 0;
				for (int32 J : Links)
				{
					if (const int32* P = NPos.Find(J))
					{
						Sum += (float)*P;
						++Count;
					}
				}
				Key.Add(Layer[K], Count > 0 ? Sum / (float)Count : (float)K);
			}
			Layer.StableSort([&](int32 A, int32 B) { return Key[A] < Key[B]; });
		};

		for (int32 Sweep = 0; Sweep < 4; ++Sweep)
		{
			for (int32 R = 1; R < Layers.Num(); ++R)
			{
				SortByBarycenter(Layers[R], Layers[R - 1], true);
			}
			for (int32 R = Layers.Num() - 2; R >= 0; --R)
			{
				SortByBarycenter(Layers[R], Layers[R + 1], false);
			}
			const int32 Crossings = CountLayerCrossings(Ctx, Layers);
			if (Crossings < BestCrossings)
			{
				BestCrossings = Crossings;
				Best = Layers;
			}
		}
		for (const TArray<int32>& Layer : Best)
		{
			for (int32 K = 0; K < Layer.Num(); ++K)
			{
				Ctx.Infos[Layer[K]].Order = K;
			}
		}
		return Best;
	}

	/** Pin pair linking Pred to Node (Pred output -> Node input) in the lane's link kind. */
	bool FindLinkPins(const FNodeInfo& Pred, const FNodeInfo& Node, bool bDataLane, UEdGraphPin*& OutPin, UEdGraphPin*& InPin)
	{
		for (UEdGraphPin* Pin : Pred.Node->Pins)
		{
			if (!Pin || Pin->Direction != EGPD_Output || IsExecPin(Pin) == bDataLane)
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked && Linked->GetOwningNode() == Node.Node)
				{
					OutPin = Pin;
					InPin = Linked;
					return true;
				}
			}
		}
		return false;
	}

	/** Places one lane with its top at LaneTop; returns the lane's bottom. */
	float PlaceLane(FLayoutContext& Ctx, int32 LaneIdx, const TArray<TArray<int32>>& Layers, float X0, float LaneTop)
	{
		const FUeaboOrganizeOptions& O = *Ctx.Options;
		const FLane& Lane = Ctx.Lanes[LaneIdx];
		const int32 NumRanks = Layers.Num();

		// Gutter column widths per rank and depth.
		TArray<TArray<float>> Gutter;
		Gutter.SetNum(NumRanks);
		for (int32 I : Lane.Data)
		{
			const FNodeInfo& D = Ctx.Infos[I];
			TArray<float>& G = Gutter[D.Rank];
			if (G.Num() < D.Depth)
			{
				G.SetNumZeroed(D.Depth);
			}
			G[D.Depth - 1] = FMath::Max(G[D.Depth - 1], D.W);
		}
		TArray<float> ColX;
		ColX.SetNum(NumRanks);
		float Cursor = X0;
		for (int32 R = 0; R < NumRanks; ++R)
		{
			for (float GW : Gutter[R])
			{
				Cursor += GW + O.HorizontalSpacing;
			}
			ColX[R] = Cursor;
			float MaxW = 0.f;
			for (int32 I : Layers[R])
			{
				MaxW = FMath::Max(MaxW, Ctx.Infos[I].W);
			}
			Cursor += MaxW + O.HorizontalSpacing;
		}

		// Chain nodes, rank by rank.
		for (int32 R = 0; R < NumRanks; ++R)
		{
			float NextFree = LaneTop;
			for (int32 I : Layers[R])
			{
				FNodeInfo& Info = Ctx.Infos[I];
				float Desired = LaneTop;
				if (O.bStraightenLinks && Info.Pred.Num() > 0)
				{
					int32 Primary = Info.Pred[0];
					for (int32 P : Info.Pred)
					{
						const FNodeInfo& PredInfo = Ctx.Infos[P];
						const FNodeInfo& Cur = Ctx.Infos[Primary];
						if (PredInfo.Rank > Cur.Rank || (PredInfo.Rank == Cur.Rank && PredInfo.Order < Cur.Order))
						{
							Primary = P;
						}
					}
					UEdGraphPin* OutPin = nullptr;
					UEdGraphPin* InPin = nullptr;
					if (FindLinkPins(Ctx.Infos[Primary], Info, Lane.bDataLane, OutPin, InPin))
					{
						Desired = Ctx.Infos[Primary].Y + PinOffsetY(OutPin) - PinOffsetY(InPin);
					}
				}
				Info.X = ColX[R];
				Info.Y = FMath::Max(Desired, NextFree);
				NextFree = Info.Y + Info.H + O.VerticalSpacing;
			}
		}

		// Data gutters, depth by depth so anchors are placed first.
		int32 MaxDepth = 0;
		for (int32 I : Lane.Data)
		{
			MaxDepth = FMath::Max(MaxDepth, Ctx.Infos[I].Depth);
		}
		for (int32 Depth = 1; Depth <= MaxDepth; ++Depth)
		{
			for (int32 R = 0; R < NumRanks; ++R)
			{
				TArray<int32> Column;
				for (int32 I : Lane.Data)
				{
					if (Ctx.Infos[I].Rank == R && Ctx.Infos[I].Depth == Depth)
					{
						Column.Add(I);
					}
				}
				if (Column.Num() == 0)
				{
					continue;
				}
				float Right = ColX[R];
				for (int32 K = 0; K < Depth; ++K)
				{
					Right -= Gutter[R][K] + O.HorizontalSpacing;
				}
				TMap<int32, float> Desired;
				for (int32 I : Column)
				{
					const FNodeInfo& D = Ctx.Infos[I];
					const FNodeInfo& A = Ctx.Infos[D.Anchor];
					Desired.Add(I, O.bStraightenLinks ? A.Y + PinOffsetY(D.AnchorPin) - PinOffsetY(D.MyPin) : A.Y);
				}
				Column.StableSort([&](int32 A, int32 B) { return Desired[A] < Desired[B]; });
				float NextFree = LaneTop;
				for (int32 I : Column)
				{
					FNodeInfo& D = Ctx.Infos[I];
					D.X = Right;
					D.Y = FMath::Max(Desired[I], NextFree);
					NextFree = D.Y + D.H + O.VerticalSpacing;
				}
			}
		}

		float Bottom = LaneTop;
		for (int32 I : Lane.Chain)
		{
			Bottom = FMath::Max(Bottom, Ctx.Infos[I].Y + Ctx.Infos[I].H);
		}
		for (int32 I : Lane.Data)
		{
			Bottom = FMath::Max(Bottom, Ctx.Infos[I].Y + Ctx.Infos[I].H);
		}
		return Bottom;
	}

	FRect LaneBounds(const FLayoutContext& Ctx, const FLane& Lane)
	{
		float MinX = TNumericLimits<float>::Max(), MinY = MinX, MaxX = -MinX, MaxY = -MinX;
		auto Grow = [&](int32 I)
		{
			const FNodeInfo& N = Ctx.Infos[I];
			MinX = FMath::Min(MinX, N.X);
			MinY = FMath::Min(MinY, N.Y);
			MaxX = FMath::Max(MaxX, N.X + N.W);
			MaxY = FMath::Max(MaxY, N.Y + N.H);
		};
		for (int32 I : Lane.Chain) { Grow(I); }
		for (int32 I : Lane.Data) { Grow(I); }
		FRect R;
		R.X = MinX; R.Y = MinY; R.W = MaxX - MinX; R.H = MaxY - MinY;
		return R;
	}

	bool SegmentHitsRect(float X1, float Y1, float X2, float Y2, const FRect& R)
	{
		// Liang-Barsky clip.
		float T0 = 0.f, T1 = 1.f;
		const float DX = X2 - X1, DY = Y2 - Y1;
		const float P[4] = { -DX, DX, -DY, DY };
		const float Q[4] = { X1 - R.X, R.Right() - X1, Y1 - R.Y, R.Bottom() - Y1 };
		for (int32 K = 0; K < 4; ++K)
		{
			if (FMath::IsNearlyZero(P[K]))
			{
				if (Q[K] < 0.f)
				{
					return false;
				}
			}
			else
			{
				const float T = Q[K] / P[K];
				if (P[K] < 0.f)
				{
					T0 = FMath::Max(T0, T);
				}
				else
				{
					T1 = FMath::Min(T1, T);
				}
			}
		}
		return T0 < T1;
	}

	bool SegmentsCross(float AX1, float AY1, float AX2, float AY2, float BX1, float BY1, float BX2, float BY2)
	{
		auto Orient = [](float PX, float PY, float QX, float QY, float RX, float RY)
		{
			const float V = (QX - PX) * (RY - PY) - (QY - PY) * (RX - PX);
			return V > 0.f ? 1 : (V < 0.f ? -1 : 0);
		};
		const int32 O1 = Orient(AX1, AY1, AX2, AY2, BX1, BY1);
		const int32 O2 = Orient(AX1, AY1, AX2, AY2, BX2, BY2);
		const int32 O3 = Orient(BX1, BY1, BX2, BY2, AX1, AY1);
		const int32 O4 = Orient(BX1, BY1, BX2, BY2, AX2, AY2);
		return O1 * O2 < 0 && O3 * O4 < 0;
	}

	struct FLink
	{
		UEdGraphPin* Out = nullptr;
		UEdGraphPin* In = nullptr;
		float X1 = 0.f, Y1 = 0.f, X2 = 0.f, Y2 = 0.f;
	};

	void GatherLinks(UEdGraph* Graph, TArray<FLink>& Out)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node || IsCommentNode(Node))
			{
				continue;
			}
			const FVector2D Size = UeaboNodeMetrics::GetNodeSize(Node);
			for (UEdGraphPin* Pin : Node->Pins)
			{
				if (!Pin || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (!Linked)
					{
						continue;
					}
					FLink L;
					L.Out = Pin;
					L.In = Linked;
					L.X1 = (float)Node->NodePosX + (float)Size.X;
					L.Y1 = (float)Node->NodePosY + PinOffsetY(Pin);
					L.X2 = (float)Linked->GetOwningNode()->NodePosX;
					L.Y2 = (float)Linked->GetOwningNode()->NodePosY + PinOffsetY(Linked);
					Out.Add(L);
				}
			}
		}
	}

	FString RootCategoryColorName(const UEdGraphNode* Root, const FString& Title)
	{
		const FString ClassName = Root->GetClass()->GetName();
		auto Has = [&](const TCHAR* Word) { return Title.Contains(Word) || ClassName.Contains(Word); };
		if (ClassName.Contains(TEXT("InputAction")) || ClassName.Contains(TEXT("InputKey")) || ClassName.Contains(TEXT("InputAxis")) || ClassName.Contains(TEXT("EnhancedInput")))
		{
			return TEXT("Input");
		}
		if (Has(TEXT("Timer")) || Has(TEXT("Delay")) || Has(TEXT("Tick")))
		{
			return TEXT("Timer");
		}
		if (Has(TEXT("Widget")) || Has(TEXT("UMG")) || Has(TEXT("HUD")) || Has(TEXT("Construct")))
		{
			return TEXT("UI");
		}
		if (Has(TEXT("Server")) || Has(TEXT("Client")) || Has(TEXT("Multicast")) || Has(TEXT("RPC")) || Has(TEXT("Replicat")) || Has(TEXT("OnRep")))
		{
			return TEXT("Network");
		}
		return Root->IsA<UK2Node_Event>() ? TEXT("Event") : TEXT("Default");
	}

	FLinearColor ColorFor(const FUeaboOrganizeOptions& O, const FString& Category)
	{
		if (Category == TEXT("Input")) { return O.InputColor; }
		if (Category == TEXT("Timer")) { return O.TimerColor; }
		if (Category == TEXT("UI")) { return O.UIColor; }
		if (Category == TEXT("Network")) { return O.NetworkColor; }
		if (Category == TEXT("Event")) { return O.EventColor; }
		return O.DefaultColor;
	}

	struct FExistingComment
	{
		UEdGraphNode_Comment* Node = nullptr;
		FRect OrigRect;
		bool bUsed = false;
	};

	/** Creates or resizes the comment titled Title around Bounds. Returns true when a node was created. */
	bool PlaceComment(UEdGraph* Graph, TArray<FExistingComment>& Existing, const FString& Title, float AnchorX, float AnchorY,
		const FRect& Bounds, float Padding, const FLinearColor& Color, FUeaboOrganizeResult& Result)
	{
		UEdGraphNode_Comment* Comment = nullptr;
		for (FExistingComment& E : Existing)
		{
			if (!E.bUsed && E.Node->NodeComment == Title)
			{
				E.bUsed = true;
				Comment = E.Node;
				break;
			}
		}
		if (!Comment)
		{
			for (FExistingComment& E : Existing)
			{
				if (!E.bUsed && E.OrigRect.Contains(AnchorX, AnchorY))
				{
					E.bUsed = true;
					Comment = E.Node;
					break;
				}
			}
		}
		const int32 X = FMath::RoundToInt(Bounds.X - Padding);
		const int32 Y = FMath::RoundToInt(Bounds.Y - Padding - 24.f);
		const int32 W = FMath::RoundToInt(Bounds.W + 2.f * Padding);
		const int32 H = FMath::RoundToInt(Bounds.H + 2.f * Padding + 24.f);
		if (Comment)
		{
			Comment->Modify();
			Comment->NodePosX = X;
			Comment->NodePosY = Y;
			Comment->NodeWidth = W;
			Comment->NodeHeight = H;
			++Result.CommentsResized;
			return false;
		}
		FGraphNodeCreator<UEdGraphNode_Comment> Creator(*Graph);
		UEdGraphNode_Comment* NewComment = Creator.CreateNode(false);
		Creator.Finalize(); // PostPlacedNewNode resets the comment text, so set everything afterwards
		NewComment->NodeComment = Title;
		NewComment->CommentColor = Color;
		NewComment->NodePosX = X;
		NewComment->NodePosY = Y;
		NewComment->NodeWidth = W;
		NewComment->NodeHeight = H;
		++Result.CommentsCreated;
		return true;
	}
}

namespace UeaboInternal
{
	FUeaboGraphStats ComputeStats(UEdGraph* Graph)
	{
		FUeaboGraphStats Stats;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (IsCommentNode(Node))
			{
				++Stats.Comments;
				continue;
			}
			++Stats.Nodes;
			if (IsOrphan(Node))
			{
				++Stats.Orphans;
			}
		}
		TArray<FLink> Links;
		GatherLinks(Graph, Links);
		for (int32 A = 0; A < Links.Num(); ++A)
		{
			for (int32 B = A + 1; B < Links.Num(); ++B)
			{
				const FLink& LA = Links[A];
				const FLink& LB = Links[B];
				if (LA.Out->GetOwningNode() == LB.Out->GetOwningNode() || LA.In->GetOwningNode() == LB.In->GetOwningNode()
					|| LA.Out->GetOwningNode() == LB.In->GetOwningNode() || LA.In->GetOwningNode() == LB.Out->GetOwningNode())
				{
					continue;
				}
				if (SegmentsCross(LA.X1, LA.Y1, LA.X2, LA.Y2, LB.X1, LB.Y1, LB.X2, LB.Y2))
				{
					++Stats.Crossings;
				}
			}
		}
		return Stats;
	}

	void OrganizeGraphBody(UBlueprint* Blueprint, UEdGraph* Graph, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result, bool& bOutStructural)
	{
		const FUeaboGraphStats Before = ComputeStats(Graph);
		Result.Before.Nodes += Before.Nodes;
		Result.Before.Crossings += Before.Crossings;
		Result.Before.Comments += Before.Comments;
		Result.Before.Orphans += Before.Orphans;

		Graph->Modify();
		if (UeaboHygiene::Run(Blueprint, Graph, Options, Result))
		{
			bOutStructural = true;
		}

		FLayoutContext Ctx;
		Ctx.Graph = Graph;
		Ctx.Options = &Options;
		TArray<FExistingComment> Existing;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}
			if (UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node))
			{
				FExistingComment E;
				E.Node = Comment;
				E.OrigRect.X = (float)Comment->NodePosX;
				E.OrigRect.Y = (float)Comment->NodePosY;
				E.OrigRect.W = (float)Comment->NodeWidth;
				E.OrigRect.H = (float)Comment->NodeHeight;
				Existing.Add(E);
				continue;
			}
			FNodeInfo Info;
			Info.Node = Node;
			const FVector2D Size = UeaboNodeMetrics::GetNodeSize(Node);
			Info.W = (float)Size.X;
			Info.H = (float)Size.Y;
			Info.OrigX = Info.X = (float)Node->NodePosX;
			Info.OrigY = Info.Y = (float)Node->NodePosY;
			Info.bExec = IsExecNode(Node);
			Ctx.Index.Add(Node, Ctx.Infos.Num());
			Ctx.Infos.Add(Info);
		}

		if (Ctx.Infos.Num() > 0)
		{
			float X0 = TNumericLimits<float>::Max();
			float Y0 = TNumericLimits<float>::Max();
			TArray<int32> ExecNodes;
			for (int32 I = 0; I < Ctx.Infos.Num(); ++I)
			{
				X0 = FMath::Min(X0, Ctx.Infos[I].OrigX);
				Y0 = FMath::Min(Y0, Ctx.Infos[I].OrigY);
				if (Ctx.Infos[I].bExec)
				{
					ExecNodes.Add(I);
				}
			}

			BuildLanes(Ctx, ExecNodes, false);
			const int32 NumExecLanes = Ctx.Lanes.Num();
			RankLanes(Ctx, 0);
			AssignDataNodes(Ctx);
			TArray<int32> Loose;
			for (int32 I = 0; I < Ctx.Infos.Num(); ++I)
			{
				if (Ctx.Infos[I].Lane == -1)
				{
					Loose.Add(I);
				}
			}
			if (Loose.Num() > 0)
			{
				const int32 First = Ctx.Lanes.Num();
				BuildLanes(Ctx, Loose, true);
				RankLanes(Ctx, First);
			}

			float LaneTop = Y0;
			for (int32 L = 0; L < Ctx.Lanes.Num(); ++L)
			{
				const TArray<TArray<int32>> Layers = OrderLane(Ctx, Ctx.Lanes[L]);
				const float Bottom = PlaceLane(Ctx, L, Layers, X0, LaneTop);
				LaneTop = Bottom + Options.VerticalSpacing * 3.f;
			}

			for (FNodeInfo& Info : Ctx.Infos)
			{
				const int32 NewX = FMath::RoundToInt(Info.X);
				const int32 NewY = FMath::RoundToInt(Info.Y);
				Info.X = (float)NewX;
				Info.Y = (float)NewY;
				if (Info.Node->NodePosX != NewX || Info.Node->NodePosY != NewY)
				{
					Info.Node->Modify();
					Info.Node->NodePosX = NewX;
					Info.Node->NodePosY = NewY;
				}
			}

			// Reroutes around nodes a straight link would cross.
			if (Options.bInsertReroutes)
			{
				const UEdGraphSchema* Schema = Graph->GetSchema();
				TArray<FRect> Occupied;
				for (const FNodeInfo& Info : Ctx.Infos)
				{
					Occupied.Add(Info.Rect());
				}
				TArray<FLink> Links;
				GatherLinks(Graph, Links);
				for (const FLink& Link : Links)
				{
					const int32 Src = Ctx.Find(Link.Out->GetOwningNode());
					const int32 Dst = Ctx.Find(Link.In->GetOwningNode());
					if (Src == INDEX_NONE || Dst == INDEX_NONE)
					{
						continue;
					}
					int32 Obstacle = INDEX_NONE;
					float BestDist = TNumericLimits<float>::Max();
					for (int32 K = 0; K < Ctx.Infos.Num(); ++K)
					{
						if (K == Src || K == Dst)
						{
							continue;
						}
						FRect R = Ctx.Infos[K].Rect();
						R.X += 2.f; R.Y += 2.f; R.W -= 4.f; R.H -= 4.f;
						if (SegmentHitsRect(Link.X1, Link.Y1, Link.X2, Link.Y2, R))
						{
							const float Dist = FMath::Abs(R.X - Link.X1);
							if (Dist < BestDist)
							{
								BestDist = Dist;
								Obstacle = K;
							}
						}
					}
					if (Obstacle == INDEX_NONE)
					{
						continue;
					}
					const FNodeInfo& Ob = Ctx.Infos[Obstacle];
					FRect Knot;
					Knot.W = 42.f;
					Knot.H = 16.f;
					Knot.X = (float)FMath::RoundToInt(Ob.X + Ob.W * 0.5f - Knot.W * 0.5f);
					Knot.Y = (float)FMath::RoundToInt(Ob.Y - Options.VerticalSpacing * 0.5f - Knot.H * 0.5f);
					if (!(Link.X1 < Knot.X && Knot.Right() < Link.X2))
					{
						continue;
					}
					FRect Padded = Knot;
					Padded.X -= 4.f; Padded.Y -= 4.f; Padded.W += 8.f; Padded.H += 8.f;
					bool bFree = true;
					for (const FRect& R : Occupied)
					{
						bFree &= !Padded.Overlaps(R);
					}
					if (!bFree)
					{
						continue;
					}
					Link.Out->GetOwningNode()->Modify();
					Link.In->GetOwningNode()->Modify();
					FGraphNodeCreator<UK2Node_Knot> Creator(*Graph);
					UK2Node_Knot* NewKnot = Creator.CreateNode(false);
					NewKnot->NodePosX = FMath::RoundToInt(Knot.X);
					NewKnot->NodePosY = FMath::RoundToInt(Knot.Y);
					Creator.Finalize();
					Schema->BreakSinglePinLink(Link.Out, Link.In);
					const bool bA = Schema->TryCreateConnection(Link.Out, NewKnot->GetInputPin());
					const bool bB = Schema->TryCreateConnection(NewKnot->GetOutputPin(), Link.In);
					if (!bA || !bB)
					{
						Result.Notes.Add(FString::Printf(TEXT("%s: reroute insertion refused by the schema for %s -> %s"), *Graph->GetName(),
							*UeaboNodeMetrics::GetTitleLine(Link.Out->GetOwningNode()), *UeaboNodeMetrics::GetTitleLine(Link.In->GetOwningNode())));
					}
					Occupied.Add(Knot);
					++Result.ReroutesInserted;
					bOutStructural = true;
				}
			}

			// Comments: one per exec lane, nested ones for same-class call clusters.
			if (Options.bCreateComments)
			{
				for (int32 L = 0; L < NumExecLanes; ++L)
				{
					const FLane& Lane = Ctx.Lanes[L];
					const FNodeInfo& Root = Ctx.Infos[Lane.Roots[0]];
					const FString Title = UeaboNodeMetrics::GetTitleLine(Root.Node);
					const FRect Bounds = LaneBounds(Ctx, Lane);
					const FLinearColor Color = ColorFor(Options, RootCategoryColorName(Root.Node, Title));
					bOutStructural |= PlaceComment(Graph, Existing, Title, Root.OrigX, Root.OrigY, Bounds, Options.CommentPadding, Color, Result);

					// Clusters: runs of consecutive exec calls to functions of one class.
					TSet<int32> Visited;
					TArray<int32> Chain = Lane.Chain;
					Chain.Sort([&](int32 A, int32 B)
					{
						const FNodeInfo& IA = Ctx.Infos[A];
						const FNodeInfo& IB = Ctx.Infos[B];
						return IA.Rank != IB.Rank ? IA.Rank < IB.Rank : IA.Order < IB.Order;
					});
					auto OwnerOf = [&](int32 I) -> UClass*
					{
						const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Ctx.Infos[I].Node);
						const UFunction* Fn = Call ? Call->GetTargetFunction() : nullptr;
						return Fn ? Fn->GetOwnerClass() : nullptr;
					};
					for (int32 Start : Chain)
					{
						UClass* Owner = OwnerOf(Start);
						if (!Owner || Visited.Contains(Start))
						{
							continue;
						}
						TArray<int32> Run;
						int32 Cur = Start;
						while (Cur != INDEX_NONE && !Visited.Contains(Cur) && OwnerOf(Cur) == Owner)
						{
							Run.Add(Cur);
							Visited.Add(Cur);
							const TArray<int32>& Succ = Ctx.Infos[Cur].Succ;
							Cur = Succ.Num() == 1 ? Succ[0] : INDEX_NONE;
						}
						if (Run.Num() > Options.ClusterCommentThreshold)
						{
							FLane Sub;
							Sub.Chain = Run;
							const FRect RunBounds = LaneBounds(Ctx, Sub);
							const FNodeInfo& First = Ctx.Infos[Run[0]];
							bOutStructural |= PlaceComment(Graph, Existing, Owner->GetName(), First.OrigX, First.OrigY, RunBounds, Options.CommentPadding * 0.5f, Options.DefaultColor, Result);
						}
					}
				}
			}

			// Suggestions.
			for (int32 L = 0; L < NumExecLanes; ++L)
			{
				const FLane& Lane = Ctx.Lanes[L];
				const FString Title = UeaboNodeMetrics::GetTitleLine(Ctx.Infos[Lane.Roots[0]].Node);
				if (Lane.MaxRank + 1 > Options.LongChainThreshold)
				{
					Result.Suggestions.Add(FString::Printf(TEXT("%s / %s: exec chain of %d nodes; consider extracting a function"), *Graph->GetName(), *Title, Lane.MaxRank + 1));
				}
				TArray<int32> Chain = Lane.Chain;
				Chain.Sort([&](int32 A, int32 B) { return Ctx.Infos[A].Rank < Ctx.Infos[B].Rank; });
				TMap<int32, int32> Depth;
				int32 MaxDepth = 0;
				for (int32 I : Chain)
				{
					int32 D = 0;
					for (int32 P : Ctx.Infos[I].Pred)
					{
						D = FMath::Max(D, Depth.FindRef(P));
					}
					D += Ctx.Infos[I].Node->IsA<UK2Node_IfThenElse>() ? 1 : 0;
					Depth.Add(I, D);
					MaxDepth = FMath::Max(MaxDepth, D);
				}
				if (MaxDepth > Options.BranchDepthThreshold)
				{
					Result.Suggestions.Add(FString::Printf(TEXT("%s / %s: %d nested Branch nodes; consider a Switch, early returns or a function"), *Graph->GetName(), *Title, MaxDepth));
				}
			}
		}

		const FUeaboGraphStats After = ComputeStats(Graph);
		Result.After.Nodes += After.Nodes;
		Result.After.Crossings += After.Crossings;
		Result.After.Comments += After.Comments;
		Result.After.Orphans += After.Orphans;
	}
}
