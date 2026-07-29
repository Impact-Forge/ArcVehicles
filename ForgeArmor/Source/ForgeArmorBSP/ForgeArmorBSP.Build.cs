// Copyright Impact-Forge. BattleSpacePlatforms adapter - stubs out when BSP is absent.

using System.IO;
using UnrealBuildTool;

public class ForgeArmorBSP : ModuleRules
{
	public ForgeArmorBSP(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayAbilities",
				"ForgeArmorCore",
			}
		);

		// BattleSpacePlatforms is BattleSpace's shipping vehicle plugin. When this
		// plugin is copied into that project the sibling exists and the adapter
		// compiles with typed access; anywhere else it is an inert stub.
		string PluginsRoot = Path.GetFullPath(Path.Combine(PluginDirectory, ".."));
		bool bWithBSP = Directory.Exists(Path.Combine(PluginsRoot, "BattleSpacePlatforms", "Source", "BattleSpacePlatforms"));

		PublicDefinitions.Add("WITH_BSPPLATFORMS=" + (bWithBSP ? "1" : "0"));

		if (bWithBSP)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"BattleSpacePlatforms",
				}
			);
		}
	}
}
