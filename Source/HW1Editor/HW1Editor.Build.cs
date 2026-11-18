using UnrealBuildTool;

public class HW1Editor : ModuleRules
{
    public HW1Editor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "HW1", "UnrealEd", "AssetRegistry",
            "AIModule", "AIGraph", "BehaviorTreeEditor", "NavigationSystem", "PCG"
        });
    }
}
