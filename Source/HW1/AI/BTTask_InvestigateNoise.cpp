#include "AI/BTTask_InvestigateNoise.h"
#include "AI/HW2AIController.h"
#include "HW1.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"

UBTTask_InvestigateNoise::UBTTask_InvestigateNoise()
{
    NodeName = TEXT("Investigate sprint noise: approach + search");
    // Runtime fields and delegate registrations must be private to each guard.
    bCreateNodeInstance = true;
    bNotifyTick = true;
    bNotifyTaskFinished = true;
    bTickIntervals = true;
}

EBTNodeResult::Type UBTTask_InvestigateNoise::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    Cleanup();
    ActiveController = Cast<AHW2AIController>(OwnerComp.GetAIOwner());
    AHW2AIController* Controller = ActiveController.Get();
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    if (!Controller || !Controller->GetPawn() || !Controller->GetPathFollowingComponent() ||
        !Board || !Board->GetValueAsBool(HW2Blackboard::HasNoise))
    {
        return EBTNodeResult::Failed;
    }
    InvestigatedRevision = Controller->GetNoiseRevision();
    if (!ReadLatestNoise(OwnerComp))
    {
        Controller->ConsumeNoise(InvestigatedRevision);
        return EBTNodeResult::Failed;
    }
    Elapsed = 0.0f;
    PauseRemaining = 0.0f;
    CompletedSearchPoints = 0;
    MoveAttempts = 0;
    bActive = true;
    bPaused = false;
    Controller->SetGuardState(EHW2GuardState::Investigate);
    MoveFinishedHandle = Controller->GetPathFollowingComponent()->OnRequestFinished.AddUObject(
        this, &UBTTask_InvestigateNoise::HandleMoveFinished);
    UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH start guard=%s radius=%.0f revision=%u"),
        *GetNameSafe(Controller->GetPawn()), Radius, InvestigatedRevision);
    StartMove(SearchCenter, true);
    SetNextTickTime(NodeMemory, 0.15f);
    return EBTNodeResult::InProgress;
}

bool UBTTask_InvestigateNoise::ReadLatestNoise(UBehaviorTreeComponent& OwnerComp)
{
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    if (!Board || !Nav || !ActiveController.IsValid())
    {
        return false;
    }
    FNavLocation Projected;
    if (!Nav->ProjectPointToNavigation(Board->GetValueAsVector(HW2Blackboard::NoiseLocation),
        Projected, FVector(200.0f, 200.0f, 300.0f)))
    {
        return false;
    }
    SearchCenter = Projected.Location;
    Radius = FMath::Clamp(Board->GetValueAsFloat(HW2Blackboard::SearchRadius), 100.0f, 900.0f);
    InvestigatedRevision = ActiveController->GetNoiseRevision();
    return true;
}

void UBTTask_InvestigateNoise::StartMove(const FVector& Destination, bool bToCenter)
{
    StopOwnedMove();
    bMovingToCenter = bToCenter;
    bPaused = false;
    bMoveCompleted = false;
    bMoveSucceeded = false;
    AHW2AIController* Controller = ActiveController.Get();
    if (!Controller)
    {
        bMoveCompleted = true;
        return;
    }
    ++MoveAttempts;
    FAIMoveRequest Request;
    Request.SetGoalLocation(Destination);
    Request.SetAcceptanceRadius(65.0f);
    Request.SetUsePathfinding(true);
    Request.SetAllowPartialPath(false);
    Request.SetProjectGoalLocation(false);
    Request.SetReachTestIncludesAgentRadius(true);
    const FPathFollowingRequestResult Result = Controller->MoveTo(Request);
    if (Result.Code == EPathFollowingRequestResult::RequestSuccessful)
    {
        ActiveMoveId = Result.MoveId;
    }
    else
    {
        bMoveCompleted = true;
        bMoveSucceeded = Result.Code == EPathFollowingRequestResult::AlreadyAtGoal;
    }
}

bool UBTTask_InvestigateNoise::StartNextSearchPoint(UBehaviorTreeComponent& OwnerComp)
{
    if (MoveAttempts >= MaximumMoveAttempts)
    {
        return false;
    }
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    AHW2AIController* Controller = ActiveController.Get();
    if (!Nav || !Controller || !Controller->GetPawn())
    {
        return false;
    }
    for (int32 Attempt = 0; Attempt < 6; ++Attempt)
    {
        FNavLocation Candidate;
        if (!Nav->GetRandomReachablePointInRadius(SearchCenter, Radius, Candidate) ||
            FVector::DistSquared2D(Candidate.Location, Controller->GetPawn()->GetActorLocation()) < FMath::Square(120.0f))
        {
            continue;
        }
        // Reachability from the sound alone is insufficient on disconnected NavMesh islands.
        // The request below also rejects partial paths from this guard's current location.
        StartMove(Candidate.Location, false);
        return true;
    }
    return false;
}

void UBTTask_InvestigateNoise::HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result)
{
    if (bActive && ActiveMoveId.IsValid() && RequestId == ActiveMoveId)
    {
        // Finish/advance on the next BT tick, not inside path-following's delegate broadcast.
        bMoveCompleted = true;
        bMoveSucceeded = Result.IsSuccess();
    }
}

void UBTTask_InvestigateNoise::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    SetNextTickTime(NodeMemory, 0.15f);
    AHW2AIController* Controller = ActiveController.Get();
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    if (!bActive || !Controller || !Controller->GetPawn() || !Board)
    {
        Cleanup();
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }
    // The Blackboard decorator normally performs this abort; this also makes the task safe in another tree.
    if (Board->GetValueAsObject(HW2Blackboard::TargetActor) || !Board->GetValueAsBool(HW2Blackboard::HasNoise))
    {
        Cleanup();
        FinishLatentTask(OwnerComp, EBTNodeResult::Aborted);
        return;
    }
    Elapsed += FMath::Max(0.0f, DeltaSeconds);
    if (Elapsed >= MaximumDuration)
    {
        FinishInvestigation(OwnerComp, TEXT("time limit"));
        return;
    }

    if (Controller->GetNoiseRevision() != InvestigatedRevision)
    {
        const FVector PreviousCenter = SearchCenter;
        const uint32 PreviousRevision = InvestigatedRevision;
        if (ReadLatestNoise(OwnerComp) && FVector::DistSquared2D(PreviousCenter, SearchCenter) > FMath::Square(100.0f))
        {
            if (MoveAttempts >= MaximumMoveAttempts)
            {
                // This search cannot approach the newer sound. Keep it pending for the next execution.
                InvestigatedRevision = PreviousRevision;
                FinishInvestigation(OwnerComp, TEXT("move attempt limit; newer noise pending"));
                return;
            }
            // A genuinely relocated sound redirects this search, but never extends its absolute time budget.
            CompletedSearchPoints = 0;
            StartMove(SearchCenter, true);
            UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH redirect revision=%u radius=%.0f"), InvestigatedRevision, Radius);
            return;
        }
    }

    if (bMoveCompleted)
    {
        bMoveCompleted = false;
        ActiveMoveId = FAIRequestID::InvalidRequest;
        if (bMoveSucceeded && !bMovingToCenter)
        {
            ++CompletedSearchPoints;
            UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH point=%d/%d guard=%s"),
                CompletedSearchPoints, SearchPointCount, *GetNameSafe(Controller->GetPawn()));
        }
        else if (!bMoveSucceeded)
        {
            UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH path failed; trying another reachable point"));
        }
        bPaused = true;
        PauseRemaining = bMoveSucceeded ? PauseAtPoint : 0.15f;
    }
    if (bPaused)
    {
        PauseRemaining -= DeltaSeconds;
        if (PauseRemaining <= 0.0f)
        {
            if (CompletedSearchPoints >= SearchPointCount)
            {
                FinishInvestigation(OwnerComp, TEXT("searched all points"));
            }
            else if (MoveAttempts >= MaximumMoveAttempts)
            {
                // The final allowed move has completed; only starting another move is forbidden.
                FinishInvestigation(OwnerComp, TEXT("move attempt limit"));
            }
            else if (!StartNextSearchPoint(OwnerComp))
            {
                FinishInvestigation(OwnerComp, TEXT("no reachable search sample"));
            }
        }
    }
}

void UBTTask_InvestigateNoise::StopOwnedMove()
{
    const FAIRequestID RequestToStop = ActiveMoveId;
    // Invalidate before AbortMove, whose completion notification may be synchronous.
    ActiveMoveId = FAIRequestID::InvalidRequest;
    if (AHW2AIController* Controller = ActiveController.Get())
    {
        UPathFollowingComponent* Following = Controller->GetPathFollowingComponent();
        if (Following && RequestToStop.IsValid() && Following->GetCurrentRequestId() == RequestToStop)
        {
            Following->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, RequestToStop);
        }
    }
}

void UBTTask_InvestigateNoise::Cleanup()
{
    bActive = false;
    if (AHW2AIController* Controller = ActiveController.Get())
    {
        if (UPathFollowingComponent* Following = Controller->GetPathFollowingComponent())
        {
            Following->OnRequestFinished.Remove(MoveFinishedHandle);
        }
    }
    MoveFinishedHandle.Reset();
    StopOwnedMove();
    ActiveController.Reset();
}

void UBTTask_InvestigateNoise::FinishInvestigation(UBehaviorTreeComponent& OwnerComp, const TCHAR* Reason)
{
    AHW2AIController* Controller = ActiveController.Get();
    const uint32 CompletedRevision = InvestigatedRevision;
    UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH finish reason=%s points=%d elapsed=%.1f"),
        Reason, CompletedSearchPoints, Elapsed);
    Cleanup();
    if (Controller)
    {
        Controller->ConsumeNoise(CompletedRevision);
        if (!Controller->GetBlackboardComponent()->GetValueAsObject(HW2Blackboard::TargetActor))
        {
            Controller->SetGuardState(EHW2GuardState::Patrol);
        }
    }
    FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_InvestigateNoise::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UE_LOG(LogHW1, Log, TEXT("HW2_SEARCH aborted; detached completion delegate and canceled owned movement"));
    Cleanup();
    return EBTNodeResult::Aborted;
}

void UBTTask_InvestigateNoise::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
    Cleanup();
    Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);
}

FString UBTTask_InvestigateNoise::GetStaticDescription() const
{
    return FString::Printf(TEXT("Approach NoiseLocation, then visit %d reachable points inside distance-dependent SearchRadius.\n"
        "New sounds redirect; sight aborts; %.0fs/%d-attempt bound; completion delegates cleaned on abort."),
        SearchPointCount, MaximumDuration, MaximumMoveAttempts);
}
