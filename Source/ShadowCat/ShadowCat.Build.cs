using UnrealBuildTool;

public class ShadowCat : ModuleRules
{
	public ShadowCat(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "UMG", "Slate", "SlateCore", "RenderCore", "RHI", "MediaAssets" });
	}
}
