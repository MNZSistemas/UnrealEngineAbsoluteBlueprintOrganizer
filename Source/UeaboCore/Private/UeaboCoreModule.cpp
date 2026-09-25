// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "IUeaboOrganizer.h"
#include "UeaboInternal.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "UeaboCore"

FUeaboOrganizeOptions::FUeaboOrganizeOptions()
	: EventColor(0.85f, 0.35f, 0.10f, 1.f)
	, InputColor(0.20f, 0.55f, 0.95f, 1.f)
	, TimerColor(0.90f, 0.75f, 0.15f, 1.f)
	, UIColor(0.55f, 0.30f, 0.85f, 1.f)
	, NetworkColor(0.15f, 0.75f, 0.45f, 1.f)
	, DefaultColor(0.45f, 0.45f, 0.45f, 1.f)
{
}

namespace
{
	bool IsK2Graph(const UEdGraph* Graph)
	{
		return Graph && Graph->GetSchema() && Graph->GetSchema()->IsA<UEdGraphSchema_K2>();
	}

	void FinishBlueprint(UBlueprint* Blueprint, bool bStructural)
	{
		if (bStructural)
		{
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		}
		else
		{
			FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
		}
	}
}

class FUeaboCoreModule : public IUeaboOrganizer
{
public:
	virtual FUeaboOrganizeResult OrganizeGraph(UEdGraph* Graph, const FUeaboOrganizeOptions& Options) override
	{
		FUeaboOrganizeResult Result;
		if (!Graph)
		{
			Result.Error = TEXT("No graph given");
			return Result;
		}
		UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
		if (!Blueprint)
		{
			Result.Error = FString::Printf(TEXT("Graph %s does not belong to a Blueprint"), *Graph->GetName());
			return Result;
		}
		if (!IsK2Graph(Graph))
		{
			Result.Error = FString::Printf(TEXT("Graph %s is not a Blueprint script graph"), *Graph->GetName());
			return Result;
		}
		const FScopedTransaction Transaction(LOCTEXT("OrganizeBlueprint", "Organize Blueprint"));
		bool bStructural = false;
		UeaboInternal::OrganizeGraphBody(Blueprint, Graph, Options, Result, bStructural);
		FinishBlueprint(Blueprint, bStructural);
		Result.bSuccess = true;
		return Result;
	}

	virtual FUeaboOrganizeResult OrganizeBlueprint(UBlueprint* Blueprint, const FUeaboOrganizeOptions& Options) override
	{
		FUeaboOrganizeResult Result;
		if (!Blueprint)
		{
			Result.Error = TEXT("No Blueprint given");
			return Result;
		}
		TArray<UEdGraph*> Graphs;
		for (UEdGraph* Graph : Blueprint->UbergraphPages) { Graphs.Add(Graph); }
		for (UEdGraph* Graph : Blueprint->FunctionGraphs) { Graphs.Add(Graph); }
		for (UEdGraph* Graph : Blueprint->MacroGraphs) { Graphs.Add(Graph); }

		const FScopedTransaction Transaction(LOCTEXT("OrganizeBlueprint", "Organize Blueprint"));
		bool bStructural = false;
		for (UEdGraph* Graph : Graphs)
		{
			if (IsK2Graph(Graph))
			{
				UeaboInternal::OrganizeGraphBody(Blueprint, Graph, Options, Result, bStructural);
			}
		}
		UeaboInternal::OrganizeMembers(Blueprint, Options, Result, bStructural);
		FinishBlueprint(Blueprint, bStructural);
		Result.bSuccess = true;
		return Result;
	}
};

IUeaboOrganizer& IUeaboOrganizer::Get()
{
	return FModuleManager::LoadModuleChecked<IUeaboOrganizer>("UeaboCore");
}

IMPLEMENT_MODULE(FUeaboCoreModule, UeaboCore)

#undef LOCTEXT_NAMESPACE
