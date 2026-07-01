// Copyright Impact-Forge. Fixed-wing / VTOL physics based on K2 FixedWing Physics by Impact-Forge.

using UnrealBuildTool;

public class ForgeVehiclesFixedWing : ModuleRules
{
	public ForgeVehiclesFixedWing(ReadOnlyTargetRules Target) : base(Target)
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
				"InputCore",
				"Slate",
				"SlateCore",
				"Chaos",
			}
			);
	}
}
