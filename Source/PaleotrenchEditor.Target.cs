using UnrealBuildTool;

public class PaleotrenchEditorTarget : TargetRules
{
	public PaleotrenchEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "Paleotrench", "PaleotrenchCore", "PaleotrenchEditor" });
	}
}
