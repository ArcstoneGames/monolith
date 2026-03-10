using UnrealBuildTool;

public class MonolithAnimation : ModuleRules
{
	public MonolithAnimation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MonolithCore",
			"UnrealEd",
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph",
			"AnimationBlueprintLibrary",
			"Json",
			"JsonUtilities",
			"PoseSearch",
			"EditorScriptingUtilities",
			"AnimationModifiers"
		});
	}
}
