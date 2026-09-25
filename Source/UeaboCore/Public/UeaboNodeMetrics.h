// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;

/** Node measurements shared by the organizer and its callers (tests, tools). */
namespace UeaboNodeMetrics
{
	/**
	 * Size the organizer uses for a node. Decided by what is present: the rendered widget size when
	 * the node is shown in an open graph editor, else the stored NodeWidth/NodeHeight when both are
	 * positive, else an estimate from the title and pin labels. Reroute knots have a fixed size.
	 */
	UEABOCORE_API FVector2D GetNodeSize(const UEdGraphNode* Node);

	/** First line of the node's full title (event name, function name...). */
	UEABOCORE_API FString GetTitleLine(const UEdGraphNode* Node);
}
