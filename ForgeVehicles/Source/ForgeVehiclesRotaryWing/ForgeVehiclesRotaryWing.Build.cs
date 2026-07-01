// Copyright Impact-Forge. Rotary-wing physics based on K2 Aircraft Physics by Kallisto.

using UnrealBuildTool;

public class ForgeVehiclesRotaryWing : ModuleRules
{
	public ForgeVehiclesRotaryWing(ReadOnlyTargetRules Target) : base(Target)
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
