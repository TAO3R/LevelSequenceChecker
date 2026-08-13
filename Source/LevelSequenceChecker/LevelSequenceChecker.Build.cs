using UnrealBuildTool;

public class LevelSequenceChecker : ModuleRules
{
	public LevelSequenceChecker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"PropertyEditor",
			"EditorFramework",
			"LevelSequence",
			"MovieScene",
			"MovieSceneTracks",
			"Sequencer",
			"AssetRegistry",
			"DeveloperSettings",
			"MessageLog",
			"ToolMenus",
			"Json",
			"JsonUtilities",
			"UniversalObjectLocator",
			"MainFrame",
		});
	}
}
