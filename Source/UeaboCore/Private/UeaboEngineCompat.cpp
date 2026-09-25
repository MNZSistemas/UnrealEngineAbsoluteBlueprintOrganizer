// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboEngineCompat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "GraphEditor.h"
#include "Layout/SlateRect.h"

namespace UeaboCompat
{
	bool GetRenderedNodeSize(const UEdGraphNode* Node, FVector2D& OutSize)
	{
		if (!Node || !Node->GetGraph())
		{
			return false;
		}
		TSharedPtr<SGraphEditor> Editor = SGraphEditor::FindGraphEditorForGraph(Node->GetGraph());
		if (!Editor.IsValid())
		{
			return false;
		}
		FSlateRect Rect;
		if (!Editor->GetBoundsForNode(Node, Rect, 0.f))
		{
			return false;
		}
		const FVector2D Size(Rect.Right - Rect.Left, Rect.Bottom - Rect.Top);
		if (Size.X <= 1.f || Size.Y <= 1.f)
		{
			return false;
		}
		OutSize = Size;
		return true;
	}

	void LinkPins(UEdGraphPin* From, UEdGraphPin* To)
	{
		From->MakeLinkTo(To);
	}
}
