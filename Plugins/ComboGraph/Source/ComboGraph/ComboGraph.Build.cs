// Copyright 2022 Mickael Daniel. All Rights Reserved.

using UnrealBuildTool;

public class ComboGraph : ModuleRules
{
	public ComboGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DeveloperSettings",
				"GameplayAbilities",
				"EnhancedInput",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"GameplayTasks",
				"Niagara",
				"Slate",
				"SlateCore",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Settings"
				}
			);
		}
	}
}
