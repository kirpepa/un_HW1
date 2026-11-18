#include "HW2WorldBuilder.h"

#include "AI/HW2AICharacter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "Async/TaskGraphInterfaces.h"
#include "BehaviorTree/BehaviorTree.h"
#include "Builders/CubeBuilder.h"
#include "Components/BrushComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Editor.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Elements/PCGTransformPoints.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Polys.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Model.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGVolume.h"
#include "Subsystems/PCGSubsystem.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogHW2World, Log, All);

namespace
{
bool SaveAsset(UObject* Asset)
{
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}

UMaterial* MakeMaterial(const TCHAR* Name, const FLinearColor& Color)
{
    const FString PackageName = FString(TEXT("/Game/HW2/Materials/")) + Name;
    UPackage* Package = CreatePackage(*PackageName);
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *(PackageName + TEXT(".") + Name));
    if (!Material)
    {
        Material = NewObject<UMaterial>(Package, Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Material);
    }
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    Data->BaseColor.UseConstant = true;
    Data->BaseColor.Constant = Color;
    Data->Roughness.UseConstant = true;
    Data->Roughness.Constant = 0.78f;
    Material->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);
    Material->PostEditChange();
    return SaveAsset(Material) ? Material : nullptr;
}

AStaticMeshActor* Box(UWorld* World, UStaticMesh* Cube, UMaterialInterface* Material,
    const TCHAR* Label, const FVector& Center, const FVector& Size, bool bCollision = true)
{
    AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
    Actor->SetActorLabel(Label);
    Actor->SetFolderPath(TEXT("Courtyard"));
    UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Static);
    Mesh->SetStaticMesh(Cube);
    Mesh->SetMaterial(0, Material);
    Mesh->SetCollisionProfileName(bCollision ? TEXT("BlockAll") : TEXT("NoCollision"));
    Mesh->SetCanEverAffectNavigation(bCollision);
    Actor->SetActorScale3D(Size / 100.0);
    return Actor;
}

void Sign(UWorld* World, const TCHAR* Label, const TCHAR* Text, const FVector& Position, float Size)
{
    ATextRenderActor* Actor = World->SpawnActor<ATextRenderActor>(Position, FRotator(0, -90, 0));
    Actor->SetActorLabel(Label);
    Actor->SetFolderPath(TEXT("Courtyard/Signs"));
    UTextRenderComponent* Component = Actor->GetTextRender();
    Component->SetText(FText::FromString(Text));
    Component->SetWorldSize(Size);
    Component->SetHorizontalAlignment(EHTA_Center);
    Component->SetVerticalAlignment(EVRTA_TextCenter);
    Component->SetTextRenderColor(FColor(224, 235, 227));
}

bool BuildVolume(UWorld* World, AVolume* Volume, const FVector& Size)
{
    Volume->Brush = NewObject<UModel>(Volume, NAME_None, RF_Transactional);
    Volume->Brush->Initialize(Volume, false);
    Volume->Brush->Polys = NewObject<UPolys>(Volume->Brush, NAME_None, RF_Transactional);
    Volume->GetBrushComponent()->Brush = Volume->Brush;
    UCubeBuilder* Builder = NewObject<UCubeBuilder>(Volume, NAME_None, RF_Transactional);
    Builder->X = Size.X;
    Builder->Y = Size.Y;
    Builder->Z = Size.Z;
    Volume->BrushBuilder = Builder;
    if (!Builder->Build(World, Volume))
    {
        return false;
    }
    Volume->GetBrushComponent()->Brush = Volume->Brush;
    Volume->GetBrushComponent()->BuildSimpleBrushCollision();
    Volume->GetBrushComponent()->UpdateBounds();
    Volume->PostEditChange();
    Volume->ReregisterAllComponents();
    return true;
}

UPCGGraph* BuildObstacleGraph(UStaticMesh* Cube, UMaterialInterface* Slate, UMaterialInterface* Teal)
{
    constexpr const TCHAR* PackageName = TEXT("/Game/HW2/PCG/PCG_Obstacles");
    UPackage* Package = CreatePackage(PackageName);
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/HW2/PCG/PCG_Obstacles.PCG_Obstacles"));
    if (!Graph)
    {
        Graph = NewObject<UPCGGraph>(Package, TEXT("PCG_Obstacles"), RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Graph);
    }
    TArray<UPCGNode*> OldNodes = Graph->GetNodes();
    Graph->RemoveNodes(OldNodes);
    UPCGCreatePointsGridSettings* Grid = nullptr;
    UPCGNode* GridNode = Graph->AddNodeOfType(Grid);
    GridNode->SetNodeTitle(TEXT("24 cover positions - open central lane"));
    GridNode->SetNodePosition(0, 0);
    Grid->GridExtents = FVector(2400, 1800, 0);
    Grid->CellSize = FVector(800, 900, 100);
    Grid->CoordinateSpace = EPCGCoordinateSpace::World;

    UPCGTransformPointsSettings* Transform = nullptr;
    UPCGNode* TransformNode = Graph->AddNodeOfType(Transform);
    TransformNode->SetNodeTitle(TEXT("Seeded position, rotation and size"));
    TransformNode->SetNodePosition(350, 0);
    Transform->OffsetMin = FVector(-65, -65, 180);
    Transform->OffsetMax = FVector(65, 65, 180);
    Transform->RotationMin = FRotator(0, -10, 0);
    Transform->RotationMax = FRotator(0, 10, 0);
    Transform->ScaleMin = FVector(3.4, 2.2, 3.6);
    Transform->ScaleMax = FVector(4.8, 3.3, 3.6);
    Transform->bUniformScale = false;
    Transform->bAbsoluteScale = true;

    UPCGStaticMeshSpawnerSettings* Spawner = nullptr;
    UPCGNode* SpawnNode = Graph->AddNodeOfType(Spawner);
    SpawnNode->SetNodeTitle(TEXT("Collision and navigation enabled"));
    SpawnNode->SetNodePosition(700, 0);
    Spawner->bSynchronousLoad = true;
    Spawner->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    UPCGMeshSelectorWeighted* Selector = CastChecked<UPCGMeshSelectorWeighted>(Spawner->MeshSelectorParameters);
    for (UMaterialInterface* Material : {Slate, Teal})
    {
        FPCGMeshSelectorWeightedEntry Entry;
        Entry.Weight = Material == Slate ? 3 : 1;
        Entry.Descriptor.StaticMesh = Cube;
        Entry.Descriptor.OverrideMaterials.Add(Material);
        Entry.Descriptor.Mobility = EComponentMobility::Static;
        Entry.Descriptor.bUseDefaultCollision = false;
        Entry.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
        Entry.Descriptor.bCanEverAffectNavigation = true;
        Entry.Descriptor.bFillCollisionUnderneathForNavmesh = true;
        Entry.Descriptor.bRasterizeAsFilledConvexVolume = true;
        Selector->MeshEntries.Add(Entry);
    }
    Graph->AddEdge(GridNode, TEXT("Out"), TransformNode, TEXT("In"));
    Graph->AddEdge(TransformNode, TEXT("Out"), SpawnNode, TEXT("In"));
    Graph->AddEdge(SpawnNode, TEXT("Out"), Graph->GetOutputNode(), TEXT("Out"));
    Graph->GetInputNode()->SetNodePosition(-300, -220);
    Graph->GetOutputNode()->SetNodePosition(1050, 0);
    return SaveAsset(Graph) ? Graph : nullptr;
}
}

bool BuildHW2World()
{
    UWorld* World = GEditor ? GEditor->NewMap(false) : nullptr;
    if (!World)
    {
        UE_LOG(LogHW2World, Error, TEXT("Could not create editor world."));
        return false;
    }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterial* Sand = MakeMaterial(TEXT("M_CourtyardSand"), FLinearColor(0.42, 0.36, 0.26));
    UMaterial* Slate = MakeMaterial(TEXT("M_CoverSlate"), FLinearColor(0.11, 0.17, 0.20));
    UMaterial* Teal = MakeMaterial(TEXT("M_CoverTeal"), FLinearColor(0.04, 0.28, 0.28));
    UMaterial* Amber = MakeMaterial(TEXT("M_LaneAmber"), FLinearColor(0.95, 0.49, 0.08));
    if (!Cube || !Sand || !Slate || !Teal || !Amber)
    {
        UE_LOG(LogHW2World, Error, TEXT("Missing base mesh or failed material save."));
        return false;
    }

    World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
    World->GetWorldSettings()->bForceNoPrecomputedLighting = true;
    Box(World, Cube, Sand, TEXT("Courtyard floor - 70 x 60 m"), FVector(0, 0, -60), FVector(7000, 6000, 120));
    Box(World, Cube, Slate, TEXT("North boundary"), FVector(0, 3000, 300), FVector(7100, 100, 600));
    Box(World, Cube, Slate, TEXT("South boundary"), FVector(0, -3000, 300), FVector(7100, 100, 600));
    Box(World, Cube, Slate, TEXT("East boundary"), FVector(3500, 0, 300), FVector(100, 6000, 600));
    Box(World, Cube, Slate, TEXT("West boundary"), FVector(-3500, 0, 300), FVector(100, 6000, 600));
    Box(World, Cube, Teal, TEXT("North coping"), FVector(0, 3000, 610), FVector(7100, 120, 20));
    Box(World, Cube, Teal, TEXT("South coping"), FVector(0, -3000, 610), FVector(7100, 120, 20));
    for (float Y : {-2050.0f, 2050.0f})
    {
        Box(World, Cube, Amber, TEXT("Perimeter lane marking"), FVector(0, Y, 1), FVector(6000, 12, 2), false);
    }
    Box(World, Cube, Teal, TEXT("Entry pad"), FVector(-2700, -2350, 2), FVector(700, 700, 4), false);
    Sign(World, TEXT("Courtyard title"), TEXT("SILENT COURTYARD"), FVector(0, 2935, 420), 95);
    Sign(World, TEXT("Courtyard instructions"), TEXT("WALK TO STAY QUIET   /   SHIFT TO MAKE NOISE   /   BREAK LINE OF SIGHT"), FVector(0, 2932, 240), 37);
    Sign(World, TEXT("Entry label"), TEXT("01  /  FIELD ENTRY"), FVector(-2400, 2934, 110), 30);

    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 1800), FRotator(-38, -32, 0));
    Sun->SetActorLabel(TEXT("Warm afternoon sun"));
    Sun->SetFolderPath(TEXT("Lighting"));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(5.0f);
    Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0, 0.91, 0.78));
    CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent())->SetAtmosphereSunLight(true);
    ASkyAtmosphere* Atmosphere = World->SpawnActor<ASkyAtmosphere>();
    Atmosphere->SetFolderPath(TEXT("Lighting"));
    ASkyLight* Sky = World->SpawnActor<ASkyLight>();
    Sky->SetFolderPath(TEXT("Lighting"));
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetIntensity(1.0f);
    Sky->GetLightComponent()->bRealTimeCapture = true;
    APostProcessVolume* Exposure = World->SpawnActor<APostProcessVolume>();
    Exposure->SetActorLabel(TEXT("Stable courtyard exposure"));
    Exposure->SetFolderPath(TEXT("Lighting"));
    Exposure->bUnbound = true;
    Exposure->Settings.bOverride_AutoExposureMinBrightness = true;
    Exposure->Settings.bOverride_AutoExposureMaxBrightness = true;
    Exposure->Settings.AutoExposureMinBrightness = 0.5f;
    Exposure->Settings.AutoExposureMaxBrightness = 0.5f;

    APlayerStart* Start = World->SpawnActor<APlayerStart>(FVector(-2700, -2100, 100), FRotator(0, 25, 0));
    Start->SetActorLabel(TEXT("Player - quiet entry"));
    Start->SetFolderPath(TEXT("Gameplay"));
    FActorSpawnParameters GuardParams;
    GuardParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AHW2AICharacter* Guard = World->SpawnActor<AHW2AICharacter>(FVector(1300, 2100, 100), FRotator(0, -90, 0), GuardParams);
    Guard->BehaviorTree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/HW2/AI/BT_Guard.BT_Guard"));
    Guard->SetActorLabel(TEXT("Guard - patrol / investigate / chase"));
    Guard->SetFolderPath(TEXT("Gameplay"));

    UPCGGraph* Graph = BuildObstacleGraph(Cube, Slate, Teal);
    if (!Graph)
    {
        return false;
    }
    APCGVolume* PCG = World->SpawnActor<APCGVolume>(FVector(0, 0, 200), FRotator::ZeroRotator);
    PCG->SetActorLabel(TEXT("PCG - seeded courtyard cover"));
    PCG->SetFolderPath(TEXT("Procedural"));
    if (!BuildVolume(World, PCG, FVector(5200, 4200, 800)))
    {
        return false;
    }
    PCG->PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PCG->PCGComponent->Seed = 21124;
    PCG->PCGComponent->SetGraph(Graph);
    PCG->PCGComponent->GenerateLocal(true);
    UPCGSubsystem* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
    const double Deadline = FPlatformTime::Seconds() + 45.0;
    while (PCG->PCGComponent->IsGenerating() && FPlatformTime::Seconds() < Deadline)
    {
        PCGSubsystem->Tick(0.016f);
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        FPlatformProcess::Sleep(0.001f);
    }
    int32 Instances = 0;
    TInlineComponentArray<UInstancedStaticMeshComponent*> Meshes(PCG);
    for (UInstancedStaticMeshComponent* Mesh : Meshes)
    {
        Instances += Mesh->GetInstanceCount();
    }
    UE_LOG(LogHW2World, Display, TEXT("PCG inspection: graph=%s, nodes=%d, generated=%d, ISM components=%d, instances=%d"),
        *Graph->GetPathName(), Graph->GetNodes().Num(), PCG->PCGComponent->bGenerated, Meshes.Num(), Instances);
    if (Instances != 24 || PCG->PCGComponent->IsGenerating())
    {
        UE_LOG(LogHW2World, Error, TEXT("PCG generation did not produce all 24 cover obstacles."));
        return false;
    }

    ANavMeshBoundsVolume* NavBounds = World->SpawnActor<ANavMeshBoundsVolume>(FVector(0, 0, 400), FRotator::ZeroRotator);
    NavBounds->SetActorLabel(TEXT("Navigation - whole courtyard"));
    NavBounds->SetFolderPath(TEXT("Gameplay"));
    if (!BuildVolume(World, NavBounds, FVector(6900, 5900, 1000)))
    {
        return false;
    }
    World->UpdateWorldComponents(true, false);
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav)
    {
        UE_LOG(LogHW2World, Error, TEXT("World has no navigation system."));
        return false;
    }
    // Asset loading puts editor navigation behind a delayed async-load lock.
    // Finish its prerequisites explicitly because commandlets do not run editor frames.
    FAssetCompilingManager::Get().FinishAllCompilation();
    // Generation can finish before mesh collision compilation. Refresh each
    // generated component after compilation so navigation exports the final ISMs.
    for (UInstancedStaticMeshComponent* Mesh : Meshes)
    {
        FNavigationSystem::UpdateComponentData(*Mesh);
    }
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
        UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    Nav->OnNavigationBoundsUpdated(NavBounds);
    // Commandlets have no editor frame loop; flush queued bounds/octree updates.
    Nav->Tick(0.016f);
    Nav->Build();
    ANavigationData* NavData = Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
    if (NavData)
    {
        NavData->EnsureBuildCompletion();
    }
    ARecastNavMesh* Recast = Cast<ARecastNavMesh>(NavData);
    FNavLocation PlayerNav, GuardNav;
    const bool bPlayerOnNav = Nav->ProjectPointToNavigation(Start->GetActorLocation(), PlayerNav, FVector(100, 100, 250));
    const bool bGuardOnNav = Nav->ProjectPointToNavigation(Guard->GetActorLocation(), GuardNav, FVector(100, 100, 250));
    UE_LOG(LogHW2World, Display, TEXT("Navigation inspection: bounds=%s, active tiles=%d, player projected=%d, guard projected=%d"),
        *NavBounds->GetComponentsBoundingBox(true).ToString(), Recast ? Recast->GetNumActiveTiles() : 0, bPlayerOnNav, bGuardOnNav);
    if (!Recast || Recast->GetNumActiveTiles() == 0 || !bPlayerOnNav || !bGuardOnNav)
    {
        return false;
    }
    int32 ExcludedCoverCenters = 0;
    for (UInstancedStaticMeshComponent* Mesh : Meshes)
    {
        UE_LOG(LogHW2World, Display, TEXT("PCG navigation export: component=%s, relevant=%d, mesh nav collision=%d, fill underneath=%d"),
            *Mesh->GetName(), Mesh->IsNavigationRelevant(), Mesh->GetStaticMesh()->GetNavCollision() != nullptr, Mesh->bFillCollisionUnderneathForNavmesh);
        for (int32 Index = 0; Index < Mesh->GetInstanceCount(); ++Index)
        {
            FTransform Instance;
            Mesh->GetInstanceTransform(Index, Instance, true);
            const FVector Center = Instance.GetLocation();
            FNavLocation UnderCover;
            ExcludedCoverCenters += !Nav->ProjectPointToNavigation(FVector(Center.X, Center.Y, 5), UnderCover, FVector(10, 10, 30));
        }
    }
    UE_LOG(LogHW2World, Display, TEXT("Navigation inspection: excluded cover centers=%d/%d"), ExcludedCoverCenters, Instances);
    if (ExcludedCoverCenters != Instances)
    {
        UE_LOG(LogHW2World, Error, TEXT("Navigation must exclude ground inside every generated obstacle."));
        return false;
    }
    const FString MapFilename = FPackageName::LongPackageNameToFilename(TEXT("/Game/HW2/Maps/L_HW2_PCG"), FPackageName::GetMapPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(MapFilename), true);
    const bool bSaved = FEditorFileUtils::SaveMap(World, MapFilename);
    UE_LOG(LogHW2World, Display, TEXT("HW2 map saved=%d: %s"), bSaved, *MapFilename);
    return bSaved;
}
