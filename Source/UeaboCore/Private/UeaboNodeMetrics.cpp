// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "UeaboNodeMetrics.h"
#include "UeaboEngineCompat.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Knot.h"

namespace
{
	int32 LongestLine(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines, false);
		int32 Longest = 0;
		for (const FString& Line : Lines)
		{
			Longest = FMath::Max(Longest, Line.Len());
		}
		return Longest;
	}
}

namespace UeaboNodeMetrics
{
	FVector2D GetNodeSize(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return FVector2D(0.f, 0.f);
		}
		FVector2D Rendered;
		if (UeaboCompat::GetRenderedNodeSize(Node, Rendered))
		{
			return Rendered;
		}
		if (Node->NodeWidth > 0 && Node->NodeHeight > 0)
		{
			return FVector2D((float)Node->NodeWidth, (float)Node->NodeHeight);
		}
		if (Node->IsA<UK2Node_Knot>())
		{
			return FVector2D(42.f, 16.f);
		}

		int32 InRows = 0;
		int32 OutRows = 0;
		int32 InLabel = 0;
		int32 OutLabel = 0;
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->bHidden)
			{
				continue;
			}
			const int32 Len = Pin->GetDisplayName().ToString().Len();
			if (Pin->Direction == EGPD_Input)
			{
				++InRows;
				InLabel = FMath::Max(InLabel, Len);
			}
			else
			{
				++OutRows;
				OutLabel = FMath::Max(OutLabel, Len);
			}
		}
		const int32 TitleChars = LongestLine(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		const int32 Chars = FMath::Max(TitleChars, InLabel + OutLabel);
		const float Width = FMath::Clamp(16.f + 8.f * (float)Chars, 120.f, 480.f);
		const float Height = 32.f + 24.f * (float)FMath::Max(InRows, OutRows);
		return FVector2D(Width, Height);
	}

	FString GetTitleLine(const UEdGraphNode* Node)
	{
		if (!Node)
		{
			return FString();
		}
		TArray<FString> Lines;
		Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().ParseIntoArrayLines(Lines, true);
		return Lines.Num() > 0 ? Lines[0].TrimStartAndEnd() : FString();
	}
}
