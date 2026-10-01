using UnrealBuildTool;

public class ShadowCatTarget : TargetRules
{
	public ShadowCatTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("ShadowCat");
	}
}
