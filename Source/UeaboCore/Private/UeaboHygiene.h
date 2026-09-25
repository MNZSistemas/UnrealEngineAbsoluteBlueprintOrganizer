// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UeaboTypes.h"

class UBlueprint;
class UEdGraph;

namespace UeaboHygiene
{
	/** No-op reroutes, duplicate casts, orphans (per options). Returns true when nodes were removed or relinked. */
	bool Run(UBlueprint* Blueprint, UEdGraph* Graph, const FUeaboOrganizeOptions& Options, FUeaboOrganizeResult& Result);
}
