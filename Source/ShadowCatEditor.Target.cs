using UnrealBuildTool;

public class ShadowCatEditorTarget : TargetRules
{
	public ShadowCatEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ShadowCat");
	}
}
