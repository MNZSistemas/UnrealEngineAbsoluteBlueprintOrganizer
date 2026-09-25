// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboHygiene.h"
#include "UeaboInternal.h"
#include "UeaboEngineCompat.h"
#include "UeaboNodeMetrics.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Knot.h"
#include "K2Node_Tunnel.h"
#include "Kismet2/BlueprintEditorUtils.h"

DEFINE_LOG_CATEGORY(LogUeabo);

namespace UeaboInternal
{
	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	bool IsExecNode(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsExecPin(Pin))
			{
				return true;
			}
		}
		return false;
	}

	bool IsCommentNode(const UEdGraphNode* Node)
	{
		return Node && Node->IsA<UEdGraphNode_Comment>();
	}

	bool IsRootNode(const UEdGraphNode* Node)
	{
		if (!IsExecNode(Node))
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (IsExecPin(Pin) && Pin->Direction == EGPD_Input)
			{
				return false;
			}
		}
		return true;
	}

	bool IsOrphan(const UEdGraphNode* Node)
	{
		if (!Node || IsCommentNode(Node) || Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>()
			|| Node->IsA<UK2Node_FunctionResult>() || Node->GetClass() == UK2Node_Tunnel::StaticClass() || IsRootNode(Node))
		{
			return false;
		}
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->LinkedTo.Num() > 0)
			{
				return false;
			}
		}
		return true;
	}

	float PinOffsetY(const UEdGraphPin* Pin)
	{
		if (!Pin)
		{
			return 0.f;
		}
		const UEdGraphNode* Node = Pin->GetOwningNode();
		if (Node->IsA<UK2Node_Knot>())
		{
			return 8.f;
		}
		int32 Index = 0;
		for (const UEdGraphPin* Other : Node->Pins)
		{
			if (Other == Pin)
			{
				break;
			}
			if (Other && !Other->bHidden && Other->Direction == Pin->Direction)
			{
				++Index;
			}
		}
		return 32.f + 24.f * (float)Index + 12.f;
	}
}

namespace
{
	using namespace UeaboInternal;

	void ExecSuccessors(const UEdGraphNode* Node, TArray<UEdGraphNode*>& Out, const UEdGraphPin* OnlyPin = nullptr)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!IsExecPin(Pin) || Pin->Direction != EGPD_Output || (OnlyPin && Pin != OnlyPin))
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked)
				{
					Out.AddUnique(Linked->GetOwningNode());
				}
			}
		}
	}

	/** Nodes reachable from the graph roots when Blocker only continues through AllowedPin. */
	TSet<UEdGraphNode*> ReachWithBlocker(UEdGraph* Graph, UEdGraphNode* Blocker, const UEdGraphPin* AllowedPin)
	{
		TSet<UEdGraphNode*> Seen;
		TArray<UEdGraphNode*> Stack;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && IsRootNode(Node))
			{
				Stack.Add(Node);
				Seen.Add(Node);
			}
		}
		while (Stack.Num() > 0)
		{
			UEdGraphNode* Node = Stack.Pop();
			TArray<UEdGraphNode*> Next;
			if (Node == Blocker)
			{
				if (AllowedPin)
				{
					ExecSuccessors(Node, Next, AllowedPin);
				}
			}
			else
			{
				ExecSuccessors(Node, Next);
			}
			for (UEdGraphNode* N : Next)
			{
				if (!Seen.Contains(N))
				{
					Seen.Add(N);
					Stack.Add(N);
				}
			}
		}
		return Seen;
	}

	int32 RemoveNoOpReroutes(UBlueprint* Blueprint, UEdGraph* Graph, FUeaboOrganizeResult& Result)
	{
		int32 Removed = 0;
		TArray<UK2Node_Knot*> Knots;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_Knot* Knot = Cast<UK2Node_Knot>(Node))
			{
				Knots.Add(Knot);
			}
		}
		for (UK2Node_Knot* Knot : Knots)
		{
			UEdGraphPin* In = Knot->GetInputPin();
			UEdGraphPin* Out = Knot->GetOutputPin();
			if (!In || !Out)
			{
				continue;
			}
			const bool bNoLinks = In->LinkedTo.Num() == 0 && Out->LinkedTo.Num() == 0;
			const bool bPassThrough = In->LinkedTo.Num() == 1 && Out->LinkedTo.Num() > 0;
			if (!bNoLinks && !bPassThrough)
			{
				Result.Notes.Add(FString::Printf(TEXT("%s: reroute node with a dangling side kept"), *Graph->GetName()));
				continue;
			}
			if (bPassThrough)
			{
				UEdGraphPin* Source = In->LinkedTo[0];
				TArray<UEdGraphPin*> Targets = Out->LinkedTo;
				Knot->Modify();
				Source->GetOwningNode()->Modify();
				for (UEdGraphPin* Target : Targets)
				{
					Target->GetOwningNode()->Modify();
				}
				In->BreakAllPinLinks();
				Out->BreakAllPinLinks();
				for (UEdGraphPin* Target : Targets)
				{
					UeaboCompat::LinkPins(Source, Target);
				}
			}
			FBlueprintEditorUtils::RemoveNode(Blueprint, Knot, true);
			++Removed;
		}
		return Removed;
	}

	int32 MergeDuplicateCasts(UBlueprint* Blueprint, UEdGraph* Graph, FUeaboOrganizeResult& Result)
	{
		int32 Merged = 0;
		bool bChanged = true;
		while (bChanged)
		{
			bChanged = false;
			TArray<UK2Node_DynamicCast*> Casts;
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node);
				if (CastNode && !CastNode->IsNodePure() && CastNode->TargetType && CastNode->GetCastSourcePin()
					&& CastNode->GetCastSourcePin()->LinkedTo.Num() == 1)
				{
					Casts.Add(CastNode);
				}
			}
			for (int32 A = 0; A < Casts.Num() && !bChanged; ++A)
			{
				for (int32 B = 0; B < Casts.Num() && !bChanged; ++B)
				{
					UK2Node_DynamicCast* First = Casts[A];
					UK2Node_DynamicCast* Later = Casts[B];
					if (A == B || First->TargetType != Later->TargetType
						|| First->GetCastSourcePin()->LinkedTo[0] != Later->GetCastSourcePin()->LinkedTo[0])
					{
						continue;
					}
					// Later must be reachable, and only through First's success exec output.
					const TSet<UEdGraphNode*> All = ReachWithBlocker(Graph, nullptr, nullptr);
					if (!All.Contains(Later) || !All.Contains(First))
					{
						continue;
					}
					const TSet<UEdGraphNode*> WithoutValid = ReachWithBlocker(Graph, First, First->GetInvalidCastPin());
					if (WithoutValid.Contains(Later))
					{
						continue;
					}
					UEdGraphPin* LaterSuccess = Later->GetBoolSuccessPin();
					if (LaterSuccess && LaterSuccess->LinkedTo.Num() > 0)
					{
						Result.Notes.Add(FString::Printf(TEXT("%s: duplicate cast to %s kept (its Success output is used)"), *Graph->GetName(), *Later->TargetType->GetName()));
						continue;
					}
					UEdGraphPin* LaterInvalid = Later->GetInvalidCastPin();
					if (LaterInvalid && LaterInvalid->LinkedTo.Num() > 0)
					{
						Result.Notes.Add(FString::Printf(TEXT("%s: unreachable Cast Failed branch of a duplicate cast to %s dropped"), *Graph->GetName(), *Later->TargetType->GetName()));
					}

					UEdGraphPin* LaterExecIn = Later->GetExecPin();
					UEdGraphPin* LaterValid = Later->GetValidCastPin();
					UEdGraphPin* LaterResult = Later->GetCastResultPin();
					UEdGraphPin* FirstResult = First->GetCastResultPin();

					TArray<UEdGraphPin*> ExecSources = LaterExecIn ? LaterExecIn->LinkedTo : TArray<UEdGraphPin*>();
					TArray<UEdGraphPin*> ExecTargets = LaterValid ? LaterValid->LinkedTo : TArray<UEdGraphPin*>();
					TArray<UEdGraphPin*> ResultTargets = LaterResult ? LaterResult->LinkedTo : TArray<UEdGraphPin*>();

					First->Modify();
					Later->Modify();
					for (UEdGraphPin* P : ExecSources) { P->GetOwningNode()->Modify(); }
					for (UEdGraphPin* P : ExecTargets) { P->GetOwningNode()->Modify(); }
					for (UEdGraphPin* P : ResultTargets) { P->GetOwningNode()->Modify(); }

					Later->BreakAllNodeLinks();
					for (UEdGraphPin* Source : ExecSources)
					{
						for (UEdGraphPin* Target : ExecTargets)
						{
							UeaboCompat::LinkPins(Source, Target);
						}
					}
					for (UEdGraphPin* Target : ResultTargets)
					{
						UeaboCompat::LinkPins(FirstResult, Target);
					}
					FBlueprintEditorUtils::RemoveNode(Blueprint, Later, true);
					++Merged;
					bChanged = true;
				}
			}
		}
		return Merged;
	}

	int32 RemoveOrphans(UBlueprint* Blueprint, UEdGraph* Graph)
	{
		TArray<UEdGraphNode*> Orphans;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (IsOrphan(Node))
			{
				Orphans.Add(Node);
			}
		}
		for (UEdGraphNode* Node : Orphans)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
		return Orphans.Num();
	}
}

namespace UeaboHygiene
{
	bool Run(UBlueprint* Blueprint, UEdGraph* Graph, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result)
	{
		int32 Changes = 0;
		if (Options.bRemoveNoOpReroutes)
		{
			const int32 N = RemoveNoOpReroutes(Blueprint, Graph, Result);
			Result.ReroutesRemoved += N;
			Changes += N;
		}
		if (Options.bMergeDuplicateCasts)
		{
			const int32 N = MergeDuplicateCasts(Blueprint, Graph, Result);
			Result.CastsMerged += N;
			Changes += N;
		}
		if (Options.bRemoveOrphanNodes)
		{
			const int32 N = RemoveOrphans(Blueprint, Graph);
			Result.OrphansRemoved += N;
			Changes += N;
		}
		return Changes > 0;
	}
}
