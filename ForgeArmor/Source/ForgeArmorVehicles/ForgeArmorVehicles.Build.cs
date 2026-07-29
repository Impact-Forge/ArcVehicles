// Copyright Impact-Forge. ForgeVehicles adapter - compiles to a stub when ForgeVehicles is absent.

using System.IO;
using UnrealBuildTool;

public class ForgeArmorVehicles : ModuleRules
{
	public ForgeArmorVehicles(ReadOnlyTargetRules Target) : base(Target)
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
				"ForgeArmorCore",
			}
		);

		// ForgeVehicles ships as a sibling plugin. When it is present the adapter
		// links against it for typed access; otherwise the class compiles as an
		// inert stub so ForgeArmor can be dropped into any project alone.
		string PluginsRoot = Path.GetFullPath(Path.Combine(PluginDirectory, ".."));
		bool bWithForgeVehicles = Directory.Exists(Path.Combine(PluginsRoot, "ForgeVehicles", "Source", "ForgeVehiclesCore"));

		PublicDefinitions.Add("WITH_FORGEVEHICLES=" + (bWithForgeVehicles ? "1" : "0"));

		if (bWithForgeVehicles)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ForgeVehiclesCore",
					"ForgeVehiclesGround",
				}
			);
		}
	}
}
