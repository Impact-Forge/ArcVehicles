// Copyright Impact-Forge. Drone airframes, flight systems and operator control.

using UnrealBuildTool;

public class ForgeVehiclesDrones : ModuleRules
{
	public ForgeVehiclesDrones(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		IWYUSupport = IWYUSupport.Full;
		CppStandard = CppStandardVersion.Latest;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"NetCore",
				"PhysicsCore",
				"GameplayTags",
				"EnhancedInput",
				"ForgeVehiclesCore",
				// Fixed-wing drones (recon UAV, loitering munition) are configured subclasses of the
				// existing fixed-wing vehicle - its aero model already scales down to small airframes.
				"ForgeVehiclesFixedWing",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Only for URotorComponent's propeller spin visuals; drones do not use the
				// rotary-wing flight model (it is a single-body helicopter approximation).
				"ForgeVehiclesRotaryWing",
			}
		);
	}
}
