using UnrealBuildTool;

public class Paleotrench : ModuleRules
{
	public Paleotrench(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"PaleotrenchCore",
		});
	}
}
