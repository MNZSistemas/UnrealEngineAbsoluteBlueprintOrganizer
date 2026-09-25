// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UeaboTypes.h"

class UBlueprint;
class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;

DECLARE_LOG_CATEGORY_EXTERN(LogUeabo, Log, All);

namespace UeaboInternal
{
	bool IsExecPin(const UEdGraphPin* Pin);
	bool IsExecNode(const UEdGraphNode* Node);
	bool IsCommentNode(const UEdGraphNode* Node);
	/** Exec node without any exec input pin: events, function/macro entries, input events. */
	bool IsRootNode(const UEdGraphNode* Node);
	bool IsOrphan(const UEdGraphNode* Node);
	/** Vertical offset of a pin from its node's top, as laid out by the organizer. */
	float PinOffsetY(const UEdGraphPin* Pin);

	FUeaboGraphStats ComputeStats(UEdGraph* Graph);

	/** Hygiene + layout + reroutes + comments + suggestions for one graph (no transaction). */
	void OrganizeGraphBody(UBlueprint* Blueprint, UEdGraph* Graph, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result, bool& bOutStructural);

	/** Variables and member graphs of a Blueprint (no transaction). */
	void OrganizeMembers(UBlueprint* Blueprint, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result, bool& bOutStructural);

	/** PascalCase form of a member name: '_' and spaces removed, each word capitalised. */
	FString ToPascalCase(const FString& Name);
}
