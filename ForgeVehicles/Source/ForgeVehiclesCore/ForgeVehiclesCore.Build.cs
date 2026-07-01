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
				// GAS stack required by the Twisted Bytes interaction interface and Arc Inventory.
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				// External plugin integrations wired onto the core vehicle.
				"TBIA_Runtime",
				"ArcInventory",
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
