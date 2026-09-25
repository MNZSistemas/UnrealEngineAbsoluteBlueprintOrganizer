// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FUeaboCommands : public TCommands<FUeaboCommands>
{
public:
	FUeaboCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> OrganizeGraph;
	TSharedPtr<FUICommandInfo> OrganizeBlueprint;
	TSharedPtr<FUICommandInfo> OpenReport;
};
