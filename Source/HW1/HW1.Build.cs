using UnrealBuildTool;

public class HW1 : ModuleRules
{
    public HW1(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "AIModule", "NavigationSystem", "GameplayTasks", "PCG"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
        PublicIncludePaths.Add("HW1");
    }
}
