// Copyright Impact-Forge. Terminal Ballistics bridge - the only module that touches TB types.

using UnrealBuildTool;

public class ForgeArmorTB : ModuleRules
{
	public ForgeArmorTB(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags",
				"PhysicsCore",
				"ForgeArmorCore",
				"TerminalBallistics",
			}
		);
	}
}
