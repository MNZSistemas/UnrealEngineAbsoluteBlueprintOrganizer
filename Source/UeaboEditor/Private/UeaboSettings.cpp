// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboSettings.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "UeaboSettings"

namespace
{
	FUeaboCategoryRule MakeRule(const TCHAR* Pattern, EUeaboMatch Match, const TCHAR* Category)
	{
		FUeaboCategoryRule Rule;
		Rule.Pattern = Pattern;
		Rule.Match = Match;
		Rule.Category = Category;
		return Rule;
	}
}

UUeaboSettings::UUeaboSettings()
	: OrganizeGraphChord(EKeys::L, false, true, true, false)
	, OrganizeBlueprintChord(EKeys::L, true, true, true, false)
{
	const FUeaboOrganizeOptions Defaults;
	EventColor = Defaults.EventColor;
	InputColor = Defaults.InputColor;
	TimerColor = Defaults.TimerColor;
	UIColor = Defaults.UIColor;
	NetworkColor = Defaults.NetworkColor;
	DefaultColor = Defaults.DefaultColor;

	CategoryRules.Add(MakeRule(TEXT("Health"), EUeaboMatch::Contains, TEXT("Stats")));
	CategoryRules.Add(MakeRule(TEXT("Speed"), EUeaboMatch::Contains, TEXT("Movement")));
	CategoryRules.Add(MakeRule(TEXT("Widget"), EUeaboMatch::Contains, TEXT("UI")));
	CategoryRules.Add(MakeRule(TEXT("Timer"), EUeaboMatch::Suffix, TEXT("Timers")));
	CategoryRules.Add(MakeRule(TEXT("Mesh"), EUeaboMatch::Suffix, TEXT("Components")));
}

FName UUeaboSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

#if WITH_EDITOR
FText UUeaboSettings::GetSectionText() const
{
	return LOCTEXT("Section", "Unreal Engine Absolute Blueprint Organizer");
}
#endif

FUeaboOrganizeOptions UUeaboSettings::ToOptions() const
{
	FUeaboOrganizeOptions O;
	O.HorizontalSpacing = HorizontalSpacing;
	O.VerticalSpacing = VerticalSpacing;
	O.bInsertReroutes = bInsertReroutes;
	O.bStraightenLinks = bStraightenLinks;
	O.bCreateComments = bCreateComments;
	O.ClusterCommentThreshold = ClusterCommentThreshold;
	O.CommentPadding = CommentPadding;
	O.EventColor = EventColor;
	O.InputColor = InputColor;
	O.TimerColor = TimerColor;
	O.UIColor = UIColor;
	O.NetworkColor = NetworkColor;
	O.DefaultColor = DefaultColor;
	O.bRemoveOrphanNodes = bRemoveOrphanNodes;
	O.bRemoveNoOpReroutes = bRemoveNoOpReroutes;
	O.bMergeDuplicateCasts = bMergeDuplicateCasts;
	O.bSortVariables = bSortVariables;
	O.bAutoCategorize = bAutoCategorize;
	O.CategoryRules = CategoryRules;
	O.bEnforceBoolPrefix = bEnforceBoolPrefix;
	O.bEnforcePascalCase = bEnforcePascalCase;
	O.bSortFunctions = bSortFunctions;
	O.LongChainThreshold = LongChainThreshold;
	O.BranchDepthThreshold = BranchDepthThreshold;
	return O;
}

#undef LOCTEXT_NAMESPACE
