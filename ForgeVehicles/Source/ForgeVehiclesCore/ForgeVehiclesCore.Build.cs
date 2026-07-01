// Copyright Impact-Forge. Based on ArcVehicles by Puny Human & Garrett Fleenor.

using UnrealBuildTool;

public class ForgeVehiclesCore : ModuleRules
{
	public ForgeVehiclesCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivatePCHHeaderFile = "Public/ForgeVehiclesCore.h";

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"PhysicsCore",
				"NetCore",
				"InputCore",
				"EnhancedInput",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore",
				"Chaos",
			}
			);

		// Forge Vehicles is built as a Modular Gameplay feature by default.
		PublicDefinitions.Add("FORGEVEHICLES_MODULAR=1");
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ModularGameplay",
				"GameFeatures",
			}
			);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
				}
				);
		}
	}
}
