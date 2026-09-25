// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UeaboVersion.h"

class UEdGraphNode;
class UEdGraphPin;

/**
 * Every engine-version difference used by the organizer lives here (compile-time branches only).
 */
namespace UeaboCompat
{
	/** Rendered size of a node shown in an open graph editor; false when no widget shows it. */
	bool GetRenderedNodeSize(const UEdGraphNode* Node, FVector2D& OutSize);

	/** Links two pins directly (no schema validation), marking the owning nodes dirty. */
	void LinkPins(UEdGraphPin* From, UEdGraphPin* To);
}
