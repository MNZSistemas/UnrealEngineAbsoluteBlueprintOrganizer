// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#include "SUeaboReport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "UeaboReport"

void SUeaboReport::Construct(const FArguments& InArgs)
{
	SAssignNew(List, SListView<TSharedPtr<FUeaboReportRow>>)
		.ListItemsSource(&Rows)
		.OnGenerateRow(this, &SUeaboReport::MakeRow);

	TSharedPtr<FUeaboReportRow> Empty = MakeShared<FUeaboReportRow>();
	Empty->Label = TEXT("No organize run yet (Ctrl+Alt+L in a Blueprint editor)");
	Rows.Add(Empty);

	ChildSlot
	[
		List.ToSharedRef()
	];
}

void SUeaboReport::SetResult(const FString& Target, const FUeaboOrganizeResult& Result)
{
	Rows.Reset();
	auto Add = [this](const FString& Label, const FString& Before, const FString& After)
	{
		TSharedPtr<FUeaboReportRow> Row = MakeShared<FUeaboReportRow>();
		Row->Label = Label;
		Row->Before = Before;
		Row->After = After;
		Rows.Add(Row);
	};
	auto Num = [](int32 V) { return FString::FromInt(V); };

	Add(Target, TEXT("Before"), TEXT("After"));
	if (!Result.bSuccess)
	{
		Add(TEXT("Error"), Result.Error, FString());
	}
	else
	{
		Add(TEXT("Nodes"), Num(Result.Before.Nodes), Num(Result.After.Nodes));
		Add(TEXT("Link crossings"), Num(Result.Before.Crossings), Num(Result.After.Crossings));
		Add(TEXT("Comments"), Num(Result.Before.Comments), Num(Result.After.Comments));
		Add(TEXT("Orphan nodes"), Num(Result.Before.Orphans), Num(Result.After.Orphans));
		Add(TEXT("Orphans removed"), FString(), Num(Result.OrphansRemoved));
		Add(TEXT("Reroutes removed / inserted"), Num(Result.ReroutesRemoved), Num(Result.ReroutesInserted));
		Add(TEXT("Casts merged"), FString(), Num(Result.CastsMerged));
		Add(TEXT("Comments created / resized"), Num(Result.CommentsCreated), Num(Result.CommentsResized));
		Add(TEXT("Variables renamed / recategorized"), Num(Result.VariablesRenamed), Num(Result.VariablesRecategorized));
		for (const FString& S : Result.Suggestions)
		{
			Add(TEXT("Suggestion"), S, FString());
		}
		for (const FString& N : Result.Notes)
		{
			Add(TEXT("Note"), N, FString());
		}
	}
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SUeaboReport::MakeRow(TSharedPtr<FUeaboReportRow> Row, const TSharedRef<STableViewBase>& Owner)
{
	return SNew(STableRow<TSharedPtr<FUeaboReportRow>>, Owner)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(4.f, 2.f)
		[
			SNew(STextBlock).Text(FText::FromString(Row->Label))
		]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(4.f, 2.f)
		[
			SNew(STextBlock).Text(FText::FromString(Row->Before)).AutoWrapText(true)
		]
		+ SHorizontalBox::Slot().FillWidth(0.2f).Padding(4.f, 2.f)
		[
			SNew(STextBlock).Text(FText::FromString(Row->After))
		]
	];
}

#undef LOCTEXT_NAMESPACE
