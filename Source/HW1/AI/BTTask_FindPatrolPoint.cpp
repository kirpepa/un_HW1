#include "AI/BTTask_FindPatrolPoint.h"
#include "AI/HW2AICharacter.h"
#include "AI/HW2AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"

UBTTask_FindPatrolPoint::UBTTask_FindPatrolPoint()
{
    NodeName = TEXT("Find reachable patrol point around spawn");
}

EBTNodeResult::Type UBTTask_FindPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AHW2AIController* Controller = Cast<AHW2AIController>(OwnerComp.GetAIOwner());
    const AHW2AICharacter* Guard = Controller ? Cast<AHW2AICharacter>(Controller->GetPawn()) : nullptr;
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    if (!Guard || !Board || !Nav)
    {
        return EBTNodeResult::Failed;
    }
    Controller->SetGuardState(EHW2GuardState::Patrol);
    const FVector Home = Board->GetValueAsVector(HW2Blackboard::HomeLocation);
    FNavLocation ProjectedHome;
    if (!Nav->ProjectPointToNavigation(Home, ProjectedHome, FVector(120.0f, 120.0f, 250.0f)))
    {
        return EBTNodeResult::Failed;
    }
    for (int32 Attempt = 0; Attempt < 8; ++Attempt)
    {
        FNavLocation Point;
        if (Nav->GetRandomReachablePointInRadius(ProjectedHome.Location, Guard->PatrolRadius, Point) &&
            FVector::DistSquared2D(Point.Location, Guard->GetActorLocation()) > FMath::Square(150.0f))
        {
            Board->SetValueAsVector(HW2Blackboard::PatrolLocation, Point.Location);
            return EBTNodeResult::Succeeded;
        }
    }
    // A small walkable island can have no sufficiently distant sample; home is still useful.
    Board->SetValueAsVector(HW2Blackboard::PatrolLocation, ProjectedHome.Location);
    return EBTNodeResult::Succeeded;
}

FString UBTTask_FindPatrolPoint::GetStaticDescription() const
{
    return TEXT("Samples reachable NavMesh points within the guard's PatrolRadius of HomeLocation; writes PatrolLocation.");
}
