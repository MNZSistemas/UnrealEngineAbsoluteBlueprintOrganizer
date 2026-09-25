// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboCommands.h"
#include "UeaboSettings.h"
#include "IUeaboOrganizer.h"
#include "SUeaboReport.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorModule.h"
#include "EdGraph/EdGraph.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "ToolMenus.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "UeaboEditor"

DEFINE_LOG_CATEGORY_STATIC(LogUeaboEditor, Log, All);

namespace
{
	const FName ReportTabId(TEXT("UeaboReport"));
}

class FUeaboEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FUeaboCommands::Register();

		FBlueprintEditorModule& Kismet = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
		FAssetEditorExtender Extender = FAssetEditorExtender::CreateRaw(this, &FUeaboEditorModule::ExtendBlueprintEditor);
		ExtenderHandle = Extender.GetHandle();
		Kismet.GetMenuExtensibilityManager()->GetExtenderDelegates().Add(Extender);

		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(ReportTabId, FOnSpawnTab::CreateRaw(this, &FUeaboEditorModule::SpawnReportTab))
			.SetDisplayName(LOCTEXT("ReportTab", "Blueprint Organizer Report"))
			.SetTooltipText(LOCTEXT("ReportTabTip", "Before/after statistics and suggestions of the last organize"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUeaboEditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UToolMenus::UnRegisterStartupCallback(this);
			UToolMenus::UnregisterOwner(this);
		}
		if (FModuleManager::Get().IsModuleLoaded("Kismet"))
		{
			FBlueprintEditorModule& Kismet = FModuleManager::GetModuleChecked<FBlueprintEditorModule>("Kismet");
			const FDelegateHandle Handle = ExtenderHandle;
			Kismet.GetMenuExtensibilityManager()->GetExtenderDelegates().RemoveAll([Handle](const FAssetEditorExtender& D) { return D.GetHandle() == Handle; });
		}
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(ReportTabId);
		}
		FUeaboCommands::Unregister();
	}

private:
	TSharedRef<FExtender> ExtendBlueprintEditor(const TSharedRef<FUICommandList> Commands, const TArray<UObject*> Objects)
	{
		UBlueprint* Blueprint = Objects.Num() > 0 ? Cast<UBlueprint>(Objects[0]) : nullptr;
		if (Blueprint)
		{
			const FUeaboCommands& C = FUeaboCommands::Get();
			const TWeakObjectPtr<UBlueprint> Weak(Blueprint);
			if (!Commands->IsActionMapped(C.OrganizeGraph))
			{
				Commands->MapAction(C.OrganizeGraph, FExecuteAction::CreateRaw(this, &FUeaboEditorModule::RunOrganize, Weak, false));
			}
			if (!Commands->IsActionMapped(C.OrganizeBlueprint))
			{
				Commands->MapAction(C.OrganizeBlueprint, FExecuteAction::CreateRaw(this, &FUeaboEditorModule::RunOrganize, Weak, true));
			}
			if (!Commands->IsActionMapped(C.OpenReport))
			{
				Commands->MapAction(C.OpenReport, FExecuteAction::CreateRaw(this, &FUeaboEditorModule::OpenReport));
			}
		}
		return MakeShared<FExtender>();
	}

	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("AssetEditor.BlueprintEditor.ToolBar");
		FToolMenuSection& Section = Toolbar->FindOrAddSection("UeaboOrganizer");
		Section.AddEntry(FToolMenuEntry::InitComboButton(
			"UeaboOrganize",
			FUIAction(),
			FNewToolMenuChoice(FNewToolMenuDelegate::CreateLambda([](UToolMenu* Menu)
			{
				FToolMenuSection& Actions = Menu->AddSection("UeaboActions", LOCTEXT("Organizer", "Blueprint Organizer"));
				Actions.AddMenuEntry(FUeaboCommands::Get().OrganizeGraph);
				Actions.AddMenuEntry(FUeaboCommands::Get().OrganizeBlueprint);
				Actions.AddMenuEntry(FUeaboCommands::Get().OpenReport);
			})),
			LOCTEXT("Organize", "Organize"),
			LOCTEXT("OrganizeTip", "Unreal Engine Absolute Blueprint Organizer")));
	}

	void RunOrganize(TWeakObjectPtr<UBlueprint> WeakBlueprint, bool bWholeBlueprint)
	{
		UBlueprint* Blueprint = WeakBlueprint.Get();
		if (!Blueprint || !GEditor)
		{
			return;
		}
		const FUeaboOrganizeOptions Options = GetDefault<UUeaboSettings>()->ToOptions();
		FUeaboOrganizeResult Result;
		FString Target;
		if (bWholeBlueprint)
		{
			Target = Blueprint->GetName();
			Result = IUeaboOrganizer::Get().OrganizeBlueprint(Blueprint, Options);
		}
		else
		{
			IAssetEditorInstance* Instance = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorForAsset(Blueprint, false);
			FBlueprintEditor* Editor = Instance ? static_cast<FBlueprintEditor*>(Instance) : nullptr;
			UEdGraph* Graph = Editor ? Editor->GetFocusedGraph() : nullptr;
			if (!Graph)
			{
				Result.Error = TEXT("No focused graph in the Blueprint editor");
			}
			else
			{
				Target = FString::Printf(TEXT("%s / %s"), *Blueprint->GetName(), *Graph->GetName());
				Result = IUeaboOrganizer::Get().OrganizeGraph(Graph, Options);
			}
		}
		Publish(Target, Result);
	}

	void Publish(const FString& Target, const FUeaboOrganizeResult& Result)
	{
		FText Message;
		if (Result.bSuccess)
		{
			Message = FText::Format(LOCTEXT("Done", "Organized {0}: crossings {1} -> {2}, {3} orphans removed, {4} casts merged"),
				FText::FromString(Target), FText::AsNumber(Result.Before.Crossings), FText::AsNumber(Result.After.Crossings),
				FText::AsNumber(Result.OrphansRemoved), FText::AsNumber(Result.CastsMerged));
		}
		else
		{
			Message = FText::Format(LOCTEXT("Failed", "Organize failed: {0}"), FText::FromString(Result.Error));
			UE_LOG(LogUeaboEditor, Warning, TEXT("Organize failed: %s"), *Result.Error);
		}
		FNotificationInfo Info(Message);
		Info.ExpireDuration = 5.f;
		FSlateNotificationManager::Get().AddNotification(Info);

		LastTarget = Target;
		LastResult = Result;
		bHasResult = true;
		if (TSharedPtr<SUeaboReport> Report = ReportWidget.Pin())
		{
			Report->SetResult(LastTarget, LastResult);
		}
		if (GetDefault<UUeaboSettings>()->bShowReportAfterOrganize)
		{
			OpenReport();
		}
	}

	void OpenReport()
	{
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(ReportTabId));
	}

	TSharedRef<SDockTab> SpawnReportTab(const FSpawnTabArgs& Args)
	{
		TSharedRef<SUeaboReport> Report = SNew(SUeaboReport);
		if (bHasResult)
		{
			Report->SetResult(LastTarget, LastResult);
		}
		ReportWidget = Report;
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				Report
			];
	}

	FDelegateHandle ExtenderHandle;
	TWeakPtr<SUeaboReport> ReportWidget;
	FString LastTarget;
	FUeaboOrganizeResult LastResult;
	bool bHasResult = false;
};

IMPLEMENT_MODULE(FUeaboEditorModule, UeaboEditor)

#undef LOCTEXT_NAMESPACE
