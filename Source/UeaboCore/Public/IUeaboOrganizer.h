// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UeaboTypes.h"

class UEdGraph;
class UBlueprint;

/**
 * Public entry point of the organizer. Usable headless: it never reads editor settings.
 * Each call opens exactly one undo transaction ("Organize Blueprint").
 */
class UEABOCORE_API IUeaboOrganizer : public IModuleInterface
{
public:
	/** Loads the "UeaboCore" module and returns it. */
	static IUeaboOrganizer& Get();

	/** Hygiene, layout, reroutes and comments for one Blueprint graph. */
	virtual FUeaboOrganizeResult OrganizeGraph(UEdGraph* Graph, const FUeaboOrganizeOptions& Options) = 0;

	/** Every graph of the Blueprint (event, function, macro graphs) plus its members. */
	virtual FUeaboOrganizeResult OrganizeBlueprint(UBlueprint* Blueprint, const FUeaboOrganizeOptions& Options) = 0;
};
