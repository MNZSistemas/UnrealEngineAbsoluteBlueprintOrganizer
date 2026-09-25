// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "UeaboTypes.h"

struct FUeaboReportRow
{
	FString Label;
	FString Before;
	FString After;
};

/** Before/after table and suggestions of the last organize. */
class SUeaboReport : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUeaboReport) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetResult(const FString& Target, const FUeaboOrganizeResult& Result);

private:
	TSharedRef<ITableRow> MakeRow(TSharedPtr<FUeaboReportRow> Row, const TSharedRef<STableViewBase>& Owner);

	TArray<TSharedPtr<FUeaboReportRow>> Rows;
	TSharedPtr<SListView<TSharedPtr<FUeaboReportRow>>> List;
};
