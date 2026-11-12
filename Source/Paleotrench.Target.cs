using UnrealBuildTool;

public class PaleotrenchTarget : TargetRules
{
	public PaleotrenchTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Paleotrench", "PaleotrenchCore" });
	}
}
