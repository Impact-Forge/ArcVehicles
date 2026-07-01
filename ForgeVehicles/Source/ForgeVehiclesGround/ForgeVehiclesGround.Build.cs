// Copyright Impact-Forge. Ground vehicle physics based on RTune by P.Kallisto.

using UnrealBuildTool;

public class ForgeVehiclesGround : ModuleRules
{
	public ForgeVehiclesGround(ReadOnlyTargetRules Target) : base(Target)
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
				"AsyncTickPhysics",
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
