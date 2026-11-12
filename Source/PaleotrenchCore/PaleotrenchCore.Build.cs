using UnrealBuildTool;

// Engine-free gameplay logic. Everything outside Private/Module/ uses only the
// C++ standard library so it can also be built and tested without Unreal
// (see Tools/CoreTests).
public class PaleotrenchCore : ModuleRules
{
	public PaleotrenchCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
	}
}
