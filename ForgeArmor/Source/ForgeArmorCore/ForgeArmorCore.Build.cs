// Copyright Impact-Forge. Core armor & vehicle damage framework - no Terminal Ballistics dependency.

using UnrealBuildTool;

public class ForgeArmorCore : ModuleRules
{
	public ForgeArmorCore(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags",
				"PhysicsCore",
				"DeveloperSettings",
			}
		);
	}
}
