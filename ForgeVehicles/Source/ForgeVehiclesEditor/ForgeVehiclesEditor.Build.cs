// Copyright Impact-Forge. Based on ArcVehicles by Puny Human & Garrett Fleenor.

using UnrealBuildTool;

public class ForgeVehiclesEditor : ModuleRules
{
	public ForgeVehiclesEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"ForgeVehiclesCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"UnrealEd",
			"PropertyEditor",
			"AssetTools",
		});
	}
}
