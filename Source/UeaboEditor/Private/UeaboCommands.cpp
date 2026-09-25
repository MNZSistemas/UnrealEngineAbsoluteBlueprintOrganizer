// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboCommands.h"
#include "UeaboSettings.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "UeaboCommands"

FUeaboCommands::FUeaboCommands()
	: TCommands<FUeaboCommands>(
		TEXT("UnrealEngineAbsoluteBlueprintOrganizer"),
		LOCTEXT("Context", "Unreal Engine Absolute Blueprint Organizer"),
		NAME_None,
		TEXT("EditorStyle"))
{
}

void FUeaboCommands::RegisterCommands()
{
	const UUeaboSettings* Settings = GetDefault<UUeaboSettings>();
	FUICommandInfo::MakeCommandInfo(AsShared(), OrganizeGraph, TEXT("OrganizeGraph"),
		LOCTEXT("OrganizeGraph", "Organize Graph"),
		LOCTEXT("OrganizeGraphTip", "Organize the focused graph: hygiene, layout, reroutes and comments"),
		FSlateIcon(), EUserInterfaceActionType::Button, Settings->OrganizeGraphChord);
	FUICommandInfo::MakeCommandInfo(AsShared(), OrganizeBlueprint, TEXT("OrganizeBlueprint"),
		LOCTEXT("OrganizeBlueprint", "Organize Blueprint"),
		LOCTEXT("OrganizeBlueprintTip", "Organize every graph of this Blueprint and its members"),
		FSlateIcon(), EUserInterfaceActionType::Button, Settings->OrganizeBlueprintChord);
	FUICommandInfo::MakeCommandInfo(AsShared(), OpenReport, TEXT("OpenReport"),
		LOCTEXT("OpenReport", "Organizer Report"),
		LOCTEXT("OpenReportTip", "Open the report of the last organize"),
		FSlateIcon(), EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
