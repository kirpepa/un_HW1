#include "HW2BehaviorBuilder.h"

#include "AI/BTTask_FindPatrolPoint.h"
#include "AI/BTTask_InvestigateNoise.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "EdGraph/EdGraphSchema.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace
{
template<typename T>
T* FindOrCreateAsset(const TCHAR* PackageName, const TCHAR* AssetName)
{
    const FString ObjectPath = FString::Printf(TEXT("%s.%s"), PackageName, AssetName);
    if (T* Existing = LoadObject<T>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn))
    {
        Existing->Modify();
        return Existing;
    }
    UPackage* Package = CreatePackage(PackageName);
    T* Asset = NewObject<T>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
    FAssetRegistryModule::AssetCreated(Asset);
    return Asset;
}

bool SaveAsset(UObject* Asset)
{
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset, *Filename, Args);
}

template<typename T>
T* AddKey(UBlackboardData* Blackboard, const TCHAR* Name, const TCHAR* Description)
{
    FBlackboardEntry& Entry = Blackboard->Keys.AddDefaulted_GetRef();
    Entry.EntryName = Name;
    Entry.EntryDescription = Description;
    Entry.EntryCategory = TEXT("Guard");
    T* Type = NewObject<T>(Blackboard);
    Entry.KeyType = Type;
    return Type;
}

void SetBlackboardKey(UObject* Node, UBlackboardData* Blackboard, const TCHAR* KeyName)
{
    // These stock-node editor properties are protected in C++, but are intentionally
    // editable through Unreal's reflected property system.
    FStructProperty* Property = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("BlackboardKey"));
    check(Property && Property->Struct == FBlackboardKeySelector::StaticStruct());
    FBlackboardKeySelector* Selector = Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node);
    Selector->SelectedKeyName = KeyName;
    Selector->ResolveSelectedKey(*Blackboard);
}

void SetByteProperty(UObject* Object, const TCHAR* Name, uint8 Value)
{
    FByteProperty* Property = FindFProperty<FByteProperty>(Object->GetClass(), Name);
    check(Property);
    Property->SetPropertyValue_InContainer(Object, Value);
}

UBTDecorator_Blackboard* IsSet(UBehaviorTree* Tree, const TCHAR* KeyName)
{
    UBTDecorator_Blackboard* Decorator = NewObject<UBTDecorator_Blackboard>(Tree);
    SetBlackboardKey(Decorator, Tree->BlackboardAsset, KeyName);
    SetByteProperty(Decorator, TEXT("BasicOperation"), EBasicKeyOperation::Set);
    SetByteProperty(Decorator, TEXT("OperationType"), EBasicKeyOperation::Set);
    SetByteProperty(Decorator, TEXT("NotifyObserver"), EBTBlackboardRestart::ResultChange);
    SetByteProperty(Decorator, TEXT("FlowAbortMode"), EBTFlowAbortMode::Both);
    Decorator->NodeName = FString::Printf(TEXT("%s is set - abort both"), KeyName);
    Decorator->BuildDescription();
    return Decorator;
}

void AddChild(UBTCompositeNode* Parent, UBTNode* Child, UBTDecorator* Decorator = nullptr)
{
    FBTCompositeChild& Link = Parent->Children.AddDefaulted_GetRef();
    Link.ChildComposite = Cast<UBTCompositeNode>(Child);
    Link.ChildTask = Cast<UBTTaskNode>(Child);
    if (Decorator)
    {
        Link.Decorators.Add(Decorator);
    }
}

UBTTask_MoveTo* MoveTo(UBehaviorTree* Tree, const TCHAR* Key, float Radius, const TCHAR* Name)
{
    UBTTask_MoveTo* Task = NewObject<UBTTask_MoveTo>(Tree);
    SetBlackboardKey(Task, Tree->BlackboardAsset, Key);
    Task->NodeName = Name;
    Task->AcceptableRadius = Radius;
    Task->bAllowPartialPath = false;
    Task->bTrackMovingGoal = true;
    Task->bObserveBlackboardValue = true;
    return Task;
}

UBTTask_Wait* Wait(UBehaviorTree* Tree, float Seconds, const TCHAR* Name)
{
    UBTTask_Wait* Task = NewObject<UBTTask_Wait>(Tree);
    Task->NodeName = Name;
    Task->WaitTime = Seconds;
    Task->RandomDeviation = 0.0f;
    return Task;
}
}

bool BuildHW2BehaviorAssets()
{
    UBlackboardData* Blackboard = FindOrCreateAsset<UBlackboardData>(
        TEXT("/Game/HW2/AI/BB_Guard"), TEXT("BB_Guard"));
    Blackboard->Keys.Reset();
    Blackboard->Parent = nullptr;
    AddKey<UBlackboardKeyType_Vector>(Blackboard, TEXT("HomeLocation"),
        TEXT("Original spawn position; patrol always stays within a radius of this point."));
    AddKey<UBlackboardKeyType_Vector>(Blackboard, TEXT("PatrolLocation"),
        TEXT("Next reachable patrol destination on the navigation mesh."));
    AddKey<UBlackboardKeyType_Object>(Blackboard, TEXT("TargetActor"),
        TEXT("Visible player. Clearing this key interrupts pursuit."))->BaseClass = AActor::StaticClass();
    AddKey<UBlackboardKeyType_Bool>(Blackboard, TEXT("HasNoise"),
        TEXT("An uninvestigated sprint sound exists. Sight has higher priority."));
    AddKey<UBlackboardKeyType_Vector>(Blackboard, TEXT("NoiseLocation"),
        TEXT("Position where the latest sound occurred, not the player's live position."));
    AddKey<UBlackboardKeyType_Float>(Blackboard, TEXT("SearchRadius"),
        TEXT("Distance-dependent uncertainty around the heard position."));
    Blackboard->UpdateKeyIDs();
    Blackboard->UpdateIfHasSynchronizedKeys();

    UBehaviorTree* Tree = FindOrCreateAsset<UBehaviorTree>(
        TEXT("/Game/HW2/AI/BT_Guard"), TEXT("BT_Guard"));
    Tree->BlackboardAsset = Blackboard;
    Tree->RootDecorators.Reset();
    Tree->RootDecoratorOps.Reset();

    UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(Tree);
    Root->NodeName = TEXT("Guard: sight > sound > patrol");
    Tree->RootNode = Root;

    UBTComposite_Selector* Chase = NewObject<UBTComposite_Selector>(Tree);
    Chase->NodeName = TEXT("Chase visible player");
    UBTComposite_Sequence* ChaseAttempt = NewObject<UBTComposite_Sequence>(Tree);
    ChaseAttempt->NodeName = TEXT("Approach and pause at target");
    UBTTask_MoveTo* ChaseMove = MoveTo(Tree, TEXT("TargetActor"), 120.0f, TEXT("Pursue visible player"));
    UBTTask_Wait* ChasePause = Wait(Tree, 0.3f, TEXT("Pause when caught"));
    UBTTask_Wait* ChaseRetry = Wait(Tree, 0.8f, TEXT("Unreachable target: wait before retry"));
    AddChild(ChaseAttempt, ChaseMove);
    AddChild(ChaseAttempt, ChasePause);
    AddChild(Chase, ChaseAttempt);
    // A failed path (for example, the player standing on an inaccessible prop)
    // must not fall through to patrol while sight still confirms the target.
    AddChild(Chase, ChaseRetry);
    AddChild(Root, Chase, IsSet(Tree, TEXT("TargetActor")));

    UBTTask_InvestigateNoise* Investigate = NewObject<UBTTask_InvestigateNoise>(Tree);
    Investigate->NodeName = TEXT("Investigate noise: source + search area");
    AddChild(Root, Investigate, IsSet(Tree, TEXT("HasNoise")));

    UBTComposite_Sequence* Patrol = NewObject<UBTComposite_Sequence>(Tree);
    Patrol->NodeName = TEXT("Patrol around home");
    UBTTask_FindPatrolPoint* FindPatrol = NewObject<UBTTask_FindPatrolPoint>(Tree);
    UBTTask_MoveTo* PatrolMove = MoveTo(Tree, TEXT("PatrolLocation"), 60.0f, TEXT("Walk to patrol point"));
    UBTTask_Wait* PatrolPause = Wait(Tree, 1.0f, TEXT("Observe surroundings"));
    AddChild(Patrol, FindPatrol);
    AddChild(Patrol, PatrolMove);
    AddChild(Patrol, PatrolPause);
    AddChild(Root, Patrol);

    // Reconstruct editor nodes from the runtime tree through the same engine entry
    // point used when opening an asset that has no editor graph yet.
    UBehaviorTreeGraph* Graph = NewObject<UBehaviorTreeGraph>(Tree, NAME_None, RF_Transactional);
    Tree->BTGraph = Graph;
    Graph->LockUpdates();
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    Graph->OnCreated();
    Graph->Initialize();

    const TMap<UObject*, FIntPoint> Positions = {
        {Root, FIntPoint(560, 180)},
        {Chase, FIntPoint(-200, 440)},
        {ChaseAttempt, FIntPoint(-400, 740)},
        {ChaseRetry, FIntPoint(0, 740)},
        {ChaseMove, FIntPoint(-600, 1020)},
        {ChasePause, FIntPoint(-200, 1020)},
        {Investigate, FIntPoint(560, 440)},
        {Patrol, FIntPoint(1320, 440)},
        {FindPatrol, FIntPoint(1000, 740)},
        {PatrolMove, FIntPoint(1360, 740)},
        {PatrolPause, FIntPoint(1720, 740)}
    };
    for (UEdGraphNode* EdNode : Graph->Nodes)
    {
        if (UBehaviorTreeGraphNode_Root* RootEdNode = Cast<UBehaviorTreeGraphNode_Root>(EdNode))
        {
            RootEdNode->BlackboardAsset = Blackboard;
            RootEdNode->NodePosX = 560;
            RootEdNode->NodePosY = 0;
        }
        if (UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(EdNode))
        {
            BTNode->UpdateNodeClassData();
            if (const FIntPoint* Position = Positions.Find(BTNode->NodeInstance.Get()))
            {
                BTNode->NodePosX = Position->X;
                BTNode->NodePosY = Position->Y;
            }
            for (UBehaviorTreeGraphNode* DecoratorNode : BTNode->Decorators)
            {
                DecoratorNode->UpdateNodeClassData();
            }
        }
    }
    // This builds execution indices, parent links, and decorator observers, not
    // merely a diagram. Left-to-right graph positions define selector priority.
    Graph->UnlockUpdates();
    Graph->UpdateAsset(UBehaviorTreeGraph::ClearDebuggerFlags);
    Graph->OnSave();

    if (Tree->RootNode != Root || Root->Children.Num() != 3 || !Blackboard->IsValid()
        || Root->Children[0].ChildComposite != Chase
        || Root->Children[1].ChildTask != Investigate
        || Root->Children[2].ChildComposite != Patrol)
    {
        UE_LOG(LogTemp, Error, TEXT("HW2: invalid generated Behavior Tree or Blackboard."));
        return false;
    }
    for (int32 BranchIndex = 0; BranchIndex < 2; ++BranchIndex)
    {
        const FBTCompositeChild& Branch = Root->Children[BranchIndex];
        if (Branch.Decorators.Num() != 1
            || Branch.Decorators[0]->GetFlowAbortMode() != EBTFlowAbortMode::Both
            || !Branch.Decorators[0]->IsFlowAbortModeValid())
        {
            UE_LOG(LogTemp, Error, TEXT("HW2: branch %d has an invalid interrupt decorator."), BranchIndex);
            return false;
        }
    }
    const bool bSaved = SaveAsset(Blackboard) && SaveAsset(Tree);
    UE_LOG(LogTemp, Display, TEXT("HW2: Behavior Tree graph has %d nodes, %d branches; saved=%s"),
        Graph->Nodes.Num(), Tree->RootNode->Children.Num(), bSaved ? TEXT("true") : TEXT("false"));
    return bSaved;
}
