// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Framework/Commands/InputChord.h"
#include "UeaboTypes.h"
#include "UeaboSettings.generated.h"

/** Editor Preferences > Plugins > Unreal Engine Absolute Blueprint Organizer. */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "Unreal Engine Absolute Blueprint Organizer"))
class UEABOEDITOR_API UUeaboSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUeaboSettings();

	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif

	/** Options for the organizer core built from these settings. */
	FUeaboOrganizeOptions ToOptions() const;

	// Shortcuts (a change needs an editor restart)
	UPROPERTY(config, EditAnywhere, Category = "Shortcuts", meta = (ConfigRestartRequired = true))
	FInputChord OrganizeGraphChord;

	UPROPERTY(config, EditAnywhere, Category = "Shortcuts", meta = (ConfigRestartRequired = true))
	FInputChord OrganizeBlueprintChord;

	// Layout
	UPROPERTY(config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float HorizontalSpacing = 80.f;

	UPROPERTY(config, EditAnywhere, Category = "Layout", meta = (ClampMin = "0"))
	float VerticalSpacing = 40.f;

	UPROPERTY(config, EditAnywhere, Category = "Layout")
	bool bInsertReroutes = true;

	UPROPERTY(config, EditAnywhere, Category = "Layout")
	bool bStraightenLinks = true;

	// Comments
	UPROPERTY(config, EditAnywhere, Category = "Comments")
	bool bCreateComments = true;

	UPROPERTY(config, EditAnywhere, Category = "Comments", meta = (ClampMin = "1"))
	int32 ClusterCommentThreshold = 8;

	UPROPERTY(config, EditAnywhere, Category = "Comments", meta = (ClampMin = "0"))
	float CommentPadding = 30.f;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor EventColor;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor InputColor;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor TimerColor;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor UIColor;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor NetworkColor;

	UPROPERTY(config, EditAnywhere, Category = "Comments")
	FLinearColor DefaultColor;

	// Hygiene
	UPROPERTY(config, EditAnywhere, Category = "Hygiene")
	bool bRemoveOrphanNodes = true;

	UPROPERTY(config, EditAnywhere, Category = "Hygiene")
	bool bRemoveNoOpReroutes = true;

	UPROPERTY(config, EditAnywhere, Category = "Hygiene")
	bool bMergeDuplicateCasts = true;

	// Members
	UPROPERTY(config, EditAnywhere, Category = "Members")
	bool bSortVariables = true;

	UPROPERTY(config, EditAnywhere, Category = "Members")
	bool bAutoCategorize = true;

	UPROPERTY(config, EditAnywhere, Category = "Members")
	TArray<FUeaboCategoryRule> CategoryRules;

	UPROPERTY(config, EditAnywhere, Category = "Members")
	bool bEnforceBoolPrefix = true;

	UPROPERTY(config, EditAnywhere, Category = "Members")
	bool bEnforcePascalCase = true;

	UPROPERTY(config, EditAnywhere, Category = "Members")
	bool bSortFunctions = true;

	// Report
	UPROPERTY(config, EditAnywhere, Category = "Report")
	bool bShowReportAfterOrganize = true;

	UPROPERTY(config, EditAnywhere, Category = "Report", meta = (ClampMin = "1"))
	int32 LongChainThreshold = 15;

	UPROPERTY(config, EditAnywhere, Category = "Report", meta = (ClampMin = "1"))
	int32 BranchDepthThreshold = 3;
};
