// Copyright Impact-Forge. Water-craft physics authored for Forge Vehicles.

using UnrealBuildTool;

public class ForgeVehiclesWaterCraft : ModuleRules
{
	public ForgeVehiclesWaterCraft(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"ForgeVehiclesCore",
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
	}
}
