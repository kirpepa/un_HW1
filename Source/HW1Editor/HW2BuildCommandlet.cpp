#include "HW2BuildCommandlet.h"
#include "HW2BehaviorBuilder.h"
#include "HW2WorldBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

UHW2BuildCommandlet::UHW2BuildCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UHW2BuildCommandlet::Main(const FString& Params)
{
    FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().SearchAllAssets(true);
    if (!BuildHW2BehaviorAssets() || !BuildHW2World())
    {
        UE_LOG(LogTemp, Error, TEXT("HW2: asset generation failed."));
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("HW2: editable Behavior Tree, Blackboard, PCG graph and level saved."));
    return 0;
}
