// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UeaboNodeMetrics.h"
#include "IUeaboOrganizer.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Math/RandomStream.h"
#include "UObject/Package.h"

namespace UeaboTest
{
	int32 Counter = 0;

	UBlueprint* MakeBlueprint(const TCHAR* Base)
	{
		const FString Name = FString::Printf(TEXT("%s_%d"), Base, ++Counter);
		UPackage* Package = CreatePackage(*FString::Printf(TEXT("/Temp/UeaboTests/%s"), *Name));
		return FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, FName(*Name), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), NAME_None);
	}

	template <typename T>
	T* Finish(FGraphNodeCreator<T>& Creator, T* Node)
	{
		Creator.Finalize();
		Node->SetFlags(RF_Transactional);
		return Node;
	}

	UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Class, const TCHAR* Function)
	{
		FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
		UK2Node_CallFunction* Node = Creator.CreateNode(false);
		Node->SetFromFunction(Class->FindFunctionByName(FName(Function)));
		return Finish(Creator, Node);
	}

	UK2Node_CustomEvent* Event(UEdGraph* Graph, const TCHAR* Name)
	{
		FGraphNodeCreator<UK2Node_CustomEvent> Creator(*Graph);
		UK2Node_CustomEvent* Node = Creator.CreateNode(false);
		Node->CustomFunctionName = FName(Name);
		return Finish(Creator, Node);
	}

	template <typename T>
	T* Simple(UEdGraph* Graph)
	{
		FGraphNodeCreator<T> Creator(*Graph);
		T* Node = Creator.CreateNode(false);
		return Finish(Creator, Node);
	}

	UEdGraphPin* ExecIn(UEdGraphNode* Node)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UEdGraphPin* ExecOut(UEdGraphNode* Node, int32 Index = 0)
	{
		int32 K = 0;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				if (K++ == Index)
				{
					return Pin;
				}
			}
		}
		return nullptr;
	}

	UEdGraphPin* Out(UEdGraphNode* Node)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				return Pin;
			}
		}
		return nullptr;
	}

	UEdGraphPin* In(UEdGraphNode* Node, const TCHAR* Name)
	{
		return Node->FindPin(FName(Name), EGPD_Input);
	}

	bool Link(FAutomationTestBase& Test, UEdGraphPin* A, UEdGraphPin* B)
	{
		if (!A || !B)
		{
			Test.AddError(TEXT("missing pin while building the fixture"));
			return false;
		}
		const bool bOk = A->GetSchema()->TryCreateConnection(A, B);
		if (!bOk)
		{
			Test.AddError(FString::Printf(TEXT("could not link %s -> %s"), *A->PinName.ToString(), *B->PinName.ToString()));
		}
		return bOk;
	}

	FUeaboOrganizeOptions DefaultOptions()
	{
		return FUeaboOrganizeOptions();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUeaboMessyBlueprintTest, "Ueabo.Organizer.MessyBlueprint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeaboMessyBlueprintTest::RunTest(const FString& Parameters)
{
	using namespace UeaboTest;
	UBlueprint* BP = MakeBlueprint(TEXT("BP_UeaboMessy"));
	if (!TestNotNull(TEXT("blueprint"), BP))
	{
		return false;
	}
	UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(BP);
	if (!TestNotNull(TEXT("event graph"), Graph))
	{
		return false;
	}
	UClass* Sys = UKismetSystemLibrary::StaticClass();
	UClass* Math = UKismetMathLibrary::StaticClass();
	UClass* Str = UKismetStringLibrary::StaticClass();

	// Alpha: Print, Delay, Branch -> (Print, Print) / (Sequence -> Print, Print)
	UK2Node_CustomEvent* Alpha = Event(Graph, TEXT("OnUeaboAlpha"));
	UEdGraphNode* P1 = Call(Graph, Sys, TEXT("PrintString"));
	UEdGraphNode* Delay = Call(Graph, Sys, TEXT("Delay"));
	UK2Node_IfThenElse* Branch = Simple<UK2Node_IfThenElse>(Graph);
	UEdGraphNode* P2 = Call(Graph, Sys, TEXT("PrintString"));
	UEdGraphNode* P3 = Call(Graph, Sys, TEXT("PrintString"));
	UK2Node_ExecutionSequence* Seq = Simple<UK2Node_ExecutionSequence>(Graph);
	UEdGraphNode* P4 = Call(Graph, Sys, TEXT("PrintString"));
	UEdGraphNode* P5 = Call(Graph, Sys, TEXT("PrintString"));
	Link(*this, ExecOut(Alpha), ExecIn(P1));
	Link(*this, ExecOut(P1), ExecIn(Delay));
	Link(*this, ExecOut(Delay), ExecIn(Branch));
	Link(*this, ExecOut(Branch, 0), ExecIn(P2));
	Link(*this, ExecOut(P2), ExecIn(P3));
	Link(*this, ExecOut(Branch, 1), ExecIn(Seq));
	Link(*this, ExecOut(Seq, 0), ExecIn(P4));
	Link(*this, ExecOut(Seq, 1), ExecIn(P5));

	// Beta: Cast -> Print -> (exec reroute) -> duplicate Cast -> Print, result used by GetActorLocation.
	UK2Node_CustomEvent* Beta = Event(Graph, TEXT("OnUeaboBeta"));
	UK2Node_Self* Self = Simple<UK2Node_Self>(Graph);
	UK2Node_DynamicCast* Cast1;
	{
		FGraphNodeCreator<UK2Node_DynamicCast> Creator(*Graph);
		Cast1 = Creator.CreateNode(false);
		Cast1->TargetType = APawn::StaticClass();
		Finish(Creator, Cast1);
	}
	UK2Node_DynamicCast* Cast2;
	{
		FGraphNodeCreator<UK2Node_DynamicCast> Creator(*Graph);
		Cast2 = Creator.CreateNode(false);
		Cast2->TargetType = APawn::StaticClass();
		Finish(Creator, Cast2);
	}
	UEdGraphNode* P6 = Call(Graph, Sys, TEXT("PrintString"));
	UEdGraphNode* P7 = Call(Graph, Sys, TEXT("PrintString"));
	UEdGraphNode* GetLoc = Call(Graph, AActor::StaticClass(), TEXT("K2_GetActorLocation"));
	UK2Node_Knot* ExecKnot = Simple<UK2Node_Knot>(Graph);
	Link(*this, ExecOut(Beta), ExecIn(Cast1));
	Link(*this, Out(Self), Cast1->GetCastSourcePin());
	Link(*this, Out(Self), Cast2->GetCastSourcePin());
	Link(*this, Cast1->GetValidCastPin(), ExecIn(P6));
	Link(*this, ExecOut(P6), ExecKnot->GetInputPin());
	Link(*this, ExecKnot->GetOutputPin(), ExecIn(Cast2));
	Link(*this, Cast2->GetValidCastPin(), ExecIn(P7));
	Link(*this, Cast2->GetCastResultPin(), In(GetLoc, TEXT("self")));

	// Gamma: prints fed by pure math, one data reroute.
	UK2Node_CustomEvent* Gamma = Event(Graph, TEXT("OnUeaboGamma"));
	UEdGraphNode* G[5];
	for (int32 K = 0; K < 5; ++K)
	{
		G[K] = Call(Graph, Sys, TEXT("PrintString"));
	}
	Link(*this, ExecOut(Gamma), ExecIn(G[0]));
	for (int32 K = 0; K + 1 < 5; ++K)
	{
		Link(*this, ExecOut(G[K]), ExecIn(G[K + 1]));
	}
	UEdGraphNode* Add1 = Call(Graph, Math, TEXT("Add_IntInt"));
	UEdGraphNode* Mul1 = Call(Graph, Math, TEXT("Multiply_IntInt"));
	UEdGraphNode* Mul2 = Call(Graph, Math, TEXT("Multiply_IntInt"));
	UEdGraphNode* Conv1 = Call(Graph, Str, TEXT("Conv_IntToString"));
	UEdGraphNode* Add2 = Call(Graph, Math, TEXT("Add_IntInt"));
	UK2Node_Knot* DataKnot = Simple<UK2Node_Knot>(Graph);
	UEdGraphNode* Conv2 = Call(Graph, Str, TEXT("Conv_IntToString"));
	Link(*this, Out(Mul1), In(Add1, TEXT("A")));
	Link(*this, Out(Mul2), In(Add1, TEXT("B")));
	Link(*this, Out(Add1), In(Conv1, TEXT("InInt")));
	Link(*this, Out(Conv1), In(G[1], TEXT("InString")));
	Link(*this, Out(Add2), DataKnot->GetInputPin());
	Link(*this, DataKnot->GetOutputPin(), In(Conv2, TEXT("InInt")));
	Link(*this, Out(Conv2), In(G[3], TEXT("InString")));

	// Orphans.
	TArray<UEdGraphNode*> Orphans;
	Orphans.Add(Call(Graph, Sys, TEXT("PrintString")));
	Orphans.Add(Call(Graph, Math, TEXT("Add_IntInt")));
	Orphans.Add(Call(Graph, Str, TEXT("Conv_IntToString")));
	Orphans.Add(Call(Graph, Sys, TEXT("Delay")));

	// Existing comment titled like the Alpha lane.
	UEdGraphNode_Comment* OldComment;
	{
		FGraphNodeCreator<UEdGraphNode_Comment> Creator(*Graph);
		OldComment = Creator.CreateNode(false);
		Finish(Creator, OldComment);
		OldComment->NodeComment = TEXT("OnUeaboAlpha");
		OldComment->NodeWidth = 400;
		OldComment->NodeHeight = 200;
	}

	// Random positions (seeded) and the record used by the undo check.
	FRandomStream Rng(20260924);
	TArray<TPair<TWeakObjectPtr<UEdGraphNode>, FIntPoint>> Recorded;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		Node->NodePosX = Rng.RandRange(-1500, 1500);
		Node->NodePosY = Rng.RandRange(-1500, 1500);
		Recorded.Add(TPair<TWeakObjectPtr<UEdGraphNode>, FIntPoint>(Node, FIntPoint(Node->NodePosX, Node->NodePosY)));
	}
	const int32 NodesBefore = Graph->Nodes.Num();
	AddInfo(FString::Printf(TEXT("fixture: %d nodes"), NodesBefore));

	const FUeaboOrganizeResult Result = IUeaboOrganizer::Get().OrganizeBlueprint(BP, DefaultOptions());
	TestTrue(FString::Printf(TEXT("organize succeeded (%s)"), *Result.Error), Result.bSuccess);
	AddInfo(FString::Printf(TEXT("crossings %d -> %d, orphans removed %d, reroutes removed %d inserted %d, casts merged %d, comments created %d resized %d"),
		Result.Before.Crossings, Result.After.Crossings, Result.OrphansRemoved, Result.ReroutesRemoved, Result.ReroutesInserted,
		Result.CastsMerged, Result.CommentsCreated, Result.CommentsResized));

	// Orphans removed.
	TestEqual(TEXT("orphans removed"), Result.OrphansRemoved, 4);
	for (UEdGraphNode* Orphan : Orphans)
	{
		TestFalse(TEXT("orphan no longer in the graph"), Graph->Nodes.Contains(Orphan));
	}
	TestEqual(TEXT("no-op reroutes removed"), Result.ReroutesRemoved, 2);

	// Casts merged.
	TestEqual(TEXT("casts merged"), Result.CastsMerged, 1);
	int32 CastCount = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		CastCount += Node->IsA<UK2Node_DynamicCast>() ? 1 : 0;
	}
	TestEqual(TEXT("one cast left"), CastCount, 1);
	TestTrue(TEXT("GetActorLocation now uses the first cast"), In(GetLoc, TEXT("self"))->LinkedTo.Contains(Cast1->GetCastResultPin()));

	// No overlapping node bounds.
	TArray<UEdGraphNode*> Solid;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node->IsA<UEdGraphNode_Comment>())
		{
			Solid.Add(Node);
		}
	}
	int32 Overlaps = 0;
	for (int32 A = 0; A < Solid.Num(); ++A)
	{
		const FVector2D SA = UeaboNodeMetrics::GetNodeSize(Solid[A]);
		for (int32 B = A + 1; B < Solid.Num(); ++B)
		{
			const FVector2D SB = UeaboNodeMetrics::GetNodeSize(Solid[B]);
			const bool bOverlap = Solid[A]->NodePosX < Solid[B]->NodePosX + SB.X && Solid[B]->NodePosX < Solid[A]->NodePosX + SA.X
				&& Solid[A]->NodePosY < Solid[B]->NodePosY + SB.Y && Solid[B]->NodePosY < Solid[A]->NodePosY + SA.Y;
			if (bOverlap)
			{
				++Overlaps;
				AddError(FString::Printf(TEXT("overlap: %s and %s"), *UeaboNodeMetrics::GetTitleLine(Solid[A]), *UeaboNodeMetrics::GetTitleLine(Solid[B])));
			}
		}
	}
	TestEqual(TEXT("overlapping nodes"), Overlaps, 0);

	// Exec links flow left to right.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked->GetOwningNode()->NodePosX <= Node->NodePosX)
				{
					AddError(FString::Printf(TEXT("exec link goes left: %s -> %s"), *UeaboNodeMetrics::GetTitleLine(Node), *UeaboNodeMetrics::GetTitleLine(Linked->GetOwningNode())));
				}
			}
		}
	}

	// A comment per event, containing it, never duplicated.
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node->IsA<UK2Node_Event>())
		{
			continue;
		}
		const FString Title = UeaboNodeMetrics::GetTitleLine(Node);
		int32 Matching = 0;
		for (UEdGraphNode* Other : Graph->Nodes)
		{
			UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Other);
			if (Comment && Comment->NodeComment == Title)
			{
				++Matching;
				const bool bContains = Node->NodePosX >= Comment->NodePosX && Node->NodePosY >= Comment->NodePosY
					&& Node->NodePosX <= Comment->NodePosX + Comment->NodeWidth && Node->NodePosY <= Comment->NodePosY + Comment->NodeHeight;
				TestTrue(FString::Printf(TEXT("comment '%s' contains its event"), *Title), bContains);
			}
		}
		TestEqual(FString::Printf(TEXT("comments titled '%s'"), *Title), Matching, 1);
	}
	TestTrue(TEXT("existing comment reused"), Graph->Nodes.Contains(OldComment) && Result.CommentsResized >= 1);

	// Undo restores the recorded positions and nodes.
	TestTrue(TEXT("undo"), GEditor && GEditor->UndoTransaction());
	TestEqual(TEXT("node count after undo"), Graph->Nodes.Num(), NodesBefore);
	int32 Moved = 0;
	for (const TPair<TWeakObjectPtr<UEdGraphNode>, FIntPoint>& Entry : Recorded)
	{
		UEdGraphNode* Node = Entry.Key.Get();
		if (!Node || !Graph->Nodes.Contains(Node) || Node->NodePosX != Entry.Value.X || Node->NodePosY != Entry.Value.Y)
		{
			++Moved;
		}
	}
	TestEqual(TEXT("nodes not restored by undo"), Moved, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUeaboMembersTest, "Ueabo.Organizer.Members",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUeaboMembersTest::RunTest(const FString& Parameters)
{
	using namespace UeaboTest;
	UBlueprint* BP = MakeBlueprint(TEXT("BP_UeaboMembers"));
	if (!TestNotNull(TEXT("blueprint"), BP))
	{
		return false;
	}
	FEdGraphPinType IntType;
	IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
	FEdGraphPinType BoolType;
	BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
	const TCHAR* IntNames[] = { TEXT("zeta"), TEXT("health_value"), TEXT("Speed"), TEXT("my_speed"), TEXT("MySpeed") };
	for (const TCHAR* Name : IntNames)
	{
		TestTrue(FString::Printf(TEXT("add %s"), Name), FBlueprintEditorUtils::AddMemberVariable(BP, FName(Name), IntType));
	}
	TestTrue(TEXT("add isDead"), FBlueprintEditorUtils::AddMemberVariable(BP, FName(TEXT("isDead")), BoolType));
	TestTrue(TEXT("add bReady"), FBlueprintEditorUtils::AddMemberVariable(BP, FName(TEXT("bReady")), BoolType));

	FUeaboOrganizeOptions Options = DefaultOptions();
	FUeaboCategoryRule Health;
	Health.Pattern = TEXT("Health");
	Health.Match = EUeaboMatch::Contains;
	Health.Category = TEXT("Stats");
	FUeaboCategoryRule Speed;
	Speed.Pattern = TEXT("Speed");
	Speed.Match = EUeaboMatch::Suffix;
	Speed.Category = TEXT("Movement");
	Options.CategoryRules.Add(Health);
	Options.CategoryRules.Add(Speed);

	const FUeaboOrganizeResult Result = IUeaboOrganizer::Get().OrganizeBlueprint(BP, Options);
	TestTrue(TEXT("organize succeeded"), Result.bSuccess);
	TestEqual(TEXT("variables renamed"), Result.VariablesRenamed, 2);
	TestEqual(TEXT("variables recategorized"), Result.VariablesRecategorized, 4);

	TArray<FString> Names;
	for (const FBPVariableDescription& Var : BP->NewVariables)
	{
		Names.Add(Var.VarName.ToString());
	}
	const FString Joined = FString::Join(Names, TEXT(","));
	TestEqual(TEXT("variable order"), Joined, FString(TEXT("bIsDead,bReady,zeta,my_speed,MySpeed,Speed,HealthValue")));

	auto CategoryOf = [BP](const TCHAR* Name)
	{
		const int32 I = FBlueprintEditorUtils::FindNewVariableIndex(BP, FName(Name));
		return I == INDEX_NONE ? FString(TEXT("<missing>")) : BP->NewVariables[I].Category.ToString();
	};
	TestEqual(TEXT("HealthValue category"), CategoryOf(TEXT("HealthValue")), FString(TEXT("Stats")));
	TestEqual(TEXT("Speed category"), CategoryOf(TEXT("Speed")), FString(TEXT("Movement")));

	bool bCollisionNoted = false;
	for (const FString& Note : Result.Notes)
	{
		bCollisionNoted |= Note.Contains(TEXT("my_speed")) && Note.Contains(TEXT("already exists"));
	}
	TestTrue(TEXT("rename collision reported as a note"), bCollisionNoted);
	return true;
}

#endif
