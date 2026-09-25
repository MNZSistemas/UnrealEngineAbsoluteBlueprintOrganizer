// Copyright (c) 2026 MNZ Sistemas. All rights reserved.

using UnrealBuildTool;

public class UeaboCore : ModuleRules
{
	public UeaboCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
#if !UE_5_1_OR_LATER
		// UE 5.0 engine headers trip C4668 (__has_feature) on current MSVC toolchains.
		bEnableUndefinedIdentifierWarnings = false;
		PrivateDefinitions.Add("__has_feature(x)=0");
#endif

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"BlueprintGraph",
			"Kismet",
		});
	}
}
