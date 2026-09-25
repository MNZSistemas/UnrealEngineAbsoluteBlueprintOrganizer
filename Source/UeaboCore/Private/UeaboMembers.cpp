// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboInternal.h"
#include "Algo/Sort.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	bool RuleMatches(const FUeaboCategoryRule& Rule, const FString& Name)
	{
		if (Rule.Pattern.IsEmpty())
		{
			return false;
		}
		switch (Rule.Match)
		{
		case EUeaboMatch::Prefix: return Name.StartsWith(Rule.Pattern);
		case EUeaboMatch::Suffix: return Name.EndsWith(Rule.Pattern);
		default: return Name.Contains(Rule.Pattern);
		}
	}

	bool IsDefaultCategory(const FText& Category)
	{
		return Category.IsEmpty() || Category.EqualTo(UEdGraphSchema_K2::VR_DefaultCategory);
	}

	/** Target name for a variable, or the same name when nothing applies. */
	FString TargetName(const FBPVariableDescription& Var, const FUeaboOrganizeOptions& O)
	{
		const FString Name = Var.VarName.ToString();
		const bool bBool = Var.VarType.PinCategory == UEdGraphSchema_K2::PC_Boolean && Var.VarType.ContainerType == EPinContainerType::None;
		if (bBool && O.bEnforceBoolPrefix)
		{
			FString Base = Name;
			if (Name.Len() > 1 && Name[0] == TEXT('b') && (FChar::IsUpper(Name[1]) || Name[1] == TEXT('_')))
			{
				Base = Name.Mid(1);
			}
			Base = O.bEnforcePascalCase ? UeaboInternal::ToPascalCase(Base) : Base;
			if (Base.Len() > 0)
			{
				Base[0] = FChar::ToUpper(Base[0]);
			}
			return TEXT("b") + Base;
		}
		if (bBool && Name.Len() > 1 && Name[0] == TEXT('b') && FChar::IsUpper(Name[1]))
		{
			return Name; // already b-prefixed PascalCase
		}
		return O.bEnforcePascalCase ? UeaboInternal::ToPascalCase(Name) : Name;
	}

	template <typename ArrayType>
	bool SortGraphsByName(ArrayType& Graphs)
	{
		TArray<FString> Before;
		for (const auto& G : Graphs)
		{
			Before.Add(G ? G->GetName() : FString());
		}
		Algo::Sort(Graphs, [](const auto& A, const auto& B)
		{
			const FString NA = A ? A->GetName() : FString();
			const FString NB = B ? B->GetName() : FString();
			return NA < NB;
		});
		for (int32 I = 0; I < Graphs.Num(); ++I)
		{
			if ((Graphs[I] ? Graphs[I]->GetName() : FString()) != Before[I])
			{
				return true;
			}
		}
		return false;
	}
}

namespace UeaboInternal
{
	FString ToPascalCase(const FString& Name)
	{
		TArray<FString> Words;
		Name.ParseIntoArray(Words, TEXT("_"), true);
		FString Out;
		for (FString& Word : Words)
		{
			Word.ReplaceInline(TEXT(" "), TEXT(""));
			if (Word.Len() == 0)
			{
				continue;
			}
			Word[0] = FChar::ToUpper(Word[0]);
			Out += Word;
		}
		return Out.IsEmpty() ? Name : Out;
	}

	void OrganizeMembers(UBlueprint* Blueprint, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result, bool& bOutStructural)
	{
		Blueprint->Modify();

		// Renames.
		if (Options.bEnforceBoolPrefix || Options.bEnforcePascalCase)
		{
			TArray<TPair<FName, FName>> Renames;
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				const FString Target = TargetName(Var, Options);
				if (Target != Var.VarName.ToString())
				{
					Renames.Add(TPair<FName, FName>(Var.VarName, FName(*Target)));
				}
			}
			for (const TPair<FName, FName>& R : Renames)
			{
				if (R.Key.ToString().Equals(R.Value.ToString(), ESearchCase::IgnoreCase))
				{
					Result.Notes.Add(FString::Printf(TEXT("Variable %s not renamed to %s: member names are case-insensitive, rename it by hand in two steps"), *R.Key.ToString(), *R.Value.ToString()));
					continue;
				}
				if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, R.Value) != INDEX_NONE)
				{
					Result.Notes.Add(FString::Printf(TEXT("Variable %s not renamed: %s already exists"), *R.Key.ToString(), *R.Value.ToString()));
					continue;
				}
				FBlueprintEditorUtils::RenameMemberVariable(Blueprint, R.Key, R.Value);
				if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, R.Value) == INDEX_NONE)
				{
					Result.Notes.Add(FString::Printf(TEXT("Variable %s could not be renamed to %s"), *R.Key.ToString(), *R.Value.ToString()));
					continue;
				}
				++Result.VariablesRenamed;
				bOutStructural = true;
			}
		}

		// Categories.
		if (Options.bAutoCategorize)
		{
			TArray<TPair<FName, FString>> Changes;
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				if (!IsDefaultCategory(Var.Category))
				{
					continue;
				}
				for (const FUeaboCategoryRule& Rule : Options.CategoryRules)
				{
					if (RuleMatches(Rule, Var.VarName.ToString()) && !Rule.Category.IsEmpty())
					{
						Changes.Add(TPair<FName, FString>(Var.VarName, Rule.Category));
						break;
					}
				}
			}
			for (const TPair<FName, FString>& C : Changes)
			{
				FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, C.Key, nullptr, FText::FromString(C.Value), true);
				++Result.VariablesRecategorized;
				bOutStructural = true;
			}
		}

		// Variable order.
		if (Options.bSortVariables)
		{
			TArray<FName> Before;
			for (const FBPVariableDescription& Var : Blueprint->NewVariables)
			{
				Before.Add(Var.VarName);
			}
			Blueprint->NewVariables.StableSort([](const FBPVariableDescription& A, const FBPVariableDescription& B)
			{
				const FString CA = A.Category.ToString();
				const FString CB = B.Category.ToString();
				if (CA != CB)
				{
					return CA < CB;
				}
				return A.VarName.ToString() < B.VarName.ToString();
			});
			for (int32 I = 0; I < Before.Num(); ++I)
			{
				if (Blueprint->NewVariables[I].VarName != Before[I])
				{
					bOutStructural = true;
					break;
				}
			}
		}

		if (Options.bSortFunctions)
		{
			bOutStructural |= SortGraphsByName(Blueprint->FunctionGraphs);
			bOutStructural |= SortGraphsByName(Blueprint->MacroGraphs);
			bOutStructural |= SortGraphsByName(Blueprint->DelegateSignatureGraphs);
		}

		// Findings reported, not changed.
		for (const FBPVariableDescription& Var : Blueprint->NewVariables)
		{
			if (!FBlueprintEditorUtils::IsVariableUsed(Blueprint, Var.VarName))
			{
				Result.Notes.Add(FString::Printf(TEXT("Variable %s is never used"), *Var.VarName.ToString()));
			}
		}
		for (UEdGraph* Graph : Blueprint->FunctionGraphs)
		{
			if (!Graph || Graph->GetFName() == UEdGraphSchema_K2::FN_UserConstructionScript)
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node);
				if (!Entry || (Entry->GetFunctionFlags() & FUNC_BlueprintPure) != 0)
				{
					continue;
				}
				UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then);
				if (Then && Then->LinkedTo.Num() == 1 && Then->LinkedTo[0]->GetOwningNode()->IsA<UK2Node_FunctionResult>())
				{
					Result.Suggestions.Add(FString::Printf(TEXT("Function %s has no exec steps; it could be marked Pure"), *Graph->GetName()));
				}
			}
		}
	}
}
