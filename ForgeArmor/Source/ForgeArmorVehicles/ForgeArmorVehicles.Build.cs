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

		// ForgeVehicles ships alongside this plugin. When it is present the adapter links against it for
		// typed access; otherwise the class compiles as an inert stub so ForgeArmor can be dropped into
		// any project alone.
		//
		// Found by searching for the descriptor rather than by testing a fixed sibling path. This guard's
		// failure mode is a silently disabled feature rather than a build error, so it must not also
		// depend on how a host project chooses to arrange its Plugins folder - grouping both plugins
		// under a shared parent is enough to break a path-based check, and nothing tells you.
		bool bWithForgeVehicles = FindsPlugin("ForgeVehicles");

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

	/**
	 * Whether a plugin descriptor exists alongside this one.
	 *
	 * Walks up from this plugin checking each ancestor for <PluginName>/<PluginName>.uplugin, so it finds
	 * the plugin whether the two sit as siblings in a repository, as siblings under a project's Plugins
	 * folder, or grouped together inside a shared parent directory.
	 *
	 * Deliberately checks named candidates instead of recursing. A recursive search would be fine when
	 * the plugin is present and found early, but on a miss it would walk an entire project tree -
	 * content, binaries and all - on every build.
	 */
	private bool FindsPlugin(string PluginName)
	{
		string Current = PluginDirectory;

		// Three levels clears a grouping folder and the Plugins folder itself.
		for (int Depth = 0; Depth < 3; ++Depth)
		{
			string Parent = Path.GetDirectoryName(Current);
			if (string.IsNullOrEmpty(Parent))
			{
				break;
			}
			Current = Parent;

			if (File.Exists(Path.Combine(Current, PluginName, PluginName + ".uplugin")))
			{
				return true;
			}
		}

		return false;
	}
}
