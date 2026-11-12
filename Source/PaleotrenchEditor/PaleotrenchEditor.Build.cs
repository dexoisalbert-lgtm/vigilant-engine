using UnrealBuildTool;

// Editor-only tooling: greybox asset bootstrap and data validation (from M1).
public class PaleotrenchEditor : ModuleRules
{
	public PaleotrenchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"Paleotrench",
			"PaleotrenchCore",
		});
	}
}
