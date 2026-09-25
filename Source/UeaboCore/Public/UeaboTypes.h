// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UeaboTypes.generated.h"

/** How a category rule pattern is matched against a variable name. */
UENUM()
enum class EUeaboMatch : uint8
{
	Prefix,
	Suffix,
	Contains,
};

/** Variables whose name matches Pattern get Category (only when their category is empty or Default). */
USTRUCT()
struct UEABOCORE_API FUeaboCategoryRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Members")
	FString Pattern;

	UPROPERTY(EditAnywhere, Category = "Members")
	EUeaboMatch Match = EUeaboMatch::Contains;

	UPROPERTY(EditAnywhere, Category = "Members")
	FString Category;
};

/** Measurements of a graph (or the sum over a Blueprint's graphs). */
USTRUCT()
struct UEABOCORE_API FUeaboGraphStats
{
	GENERATED_BODY()

	/** Non-comment nodes. */
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Nodes = 0;

	/** Pairs of links whose straight segments cross. */
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Crossings = 0;

	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Comments = 0;

	/** Nodes without any linked pin that are not events, entries, results or comments. */
	UPROPERTY(VisibleAnywhere, Category = "Stats")
	int32 Orphans = 0;
};

/** Everything the organizer consumes. The editor module fills it from the plugin settings. */
USTRUCT()
struct UEABOCORE_API FUeaboOrganizeOptions
{
	GENERATED_BODY()

	FUeaboOrganizeOptions();

	// Layout
	UPROPERTY(EditAnywhere, Category = "Layout")
	float HorizontalSpacing = 80.f;

	UPROPERTY(EditAnywhere, Category = "Layout")
	float VerticalSpacing = 40.f;

	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bInsertReroutes = true;

	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bStraightenLinks = true;

	// Comments
	UPROPERTY(EditAnywhere, Category = "Comments")
	bool bCreateComments = true;

	UPROPERTY(EditAnywhere, Category = "Comments")
	int32 ClusterCommentThreshold = 8;

	UPROPERTY(EditAnywhere, Category = "Comments")
	float CommentPadding = 30.f;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor EventColor;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor InputColor;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor TimerColor;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor UIColor;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor NetworkColor;

	UPROPERTY(EditAnywhere, Category = "Comments")
	FLinearColor DefaultColor;

	// Hygiene
	UPROPERTY(EditAnywhere, Category = "Hygiene")
	bool bRemoveOrphanNodes = true;

	UPROPERTY(EditAnywhere, Category = "Hygiene")
	bool bRemoveNoOpReroutes = true;

	UPROPERTY(EditAnywhere, Category = "Hygiene")
	bool bMergeDuplicateCasts = true;

	// Members (OrganizeBlueprint only)
	UPROPERTY(EditAnywhere, Category = "Members")
	bool bSortVariables = true;

	UPROPERTY(EditAnywhere, Category = "Members")
	bool bAutoCategorize = true;

	UPROPERTY(EditAnywhere, Category = "Members")
	TArray<FUeaboCategoryRule> CategoryRules;

	UPROPERTY(EditAnywhere, Category = "Members")
	bool bEnforceBoolPrefix = true;

	UPROPERTY(EditAnywhere, Category = "Members")
	bool bEnforcePascalCase = true;

	UPROPERTY(EditAnywhere, Category = "Members")
	bool bSortFunctions = true;

	// Report
	UPROPERTY(EditAnywhere, Category = "Report")
	int32 LongChainThreshold = 15;

	UPROPERTY(EditAnywhere, Category = "Report")
	int32 BranchDepthThreshold = 3;
};

/** Outcome of one organize call. */
USTRUCT()
struct UEABOCORE_API FUeaboOrganizeResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Result")
	bool bSuccess = false;

	/** Set when bSuccess is false. */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	FString Error;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	FUeaboGraphStats Before;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	FUeaboGraphStats After;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 OrphansRemoved = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 ReroutesRemoved = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 ReroutesInserted = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 CastsMerged = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 CommentsCreated = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 CommentsResized = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 VariablesRenamed = 0;

	UPROPERTY(VisibleAnywhere, Category = "Result")
	int32 VariablesRecategorized = 0;

	/** Refactoring candidates (long chains, deep branching, pure candidates). */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	TArray<FString> Suggestions;

	/** Hygiene findings that were reported but not changed. */
	UPROPERTY(VisibleAnywhere, Category = "Result")
	TArray<FString> Notes;
};
