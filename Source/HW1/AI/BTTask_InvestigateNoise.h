#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_InvestigateNoise.generated.h"

class AHW2AIController;
struct FPathFollowingResult;

/** A bounded, interruptible investigation: approach the sound, then visit several search points. */
UCLASS()
class HW1_API UBTTask_InvestigateNoise : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_InvestigateNoise();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual FString GetStaticDescription() const override;

    UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="1", ClampMax="12"))
    int32 SearchPointCount = 4;

    UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="2", Units="s"))
    float MaximumDuration = 16.0f;

    UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="1"))
    int32 MaximumMoveAttempts = 18;

    UPROPERTY(EditAnywhere, Category="Search", meta=(ClampMin="0", Units="s"))
    float PauseAtPoint = 0.65f;

protected:
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

private:
    TWeakObjectPtr<AHW2AIController> ActiveController;
    FDelegateHandle MoveFinishedHandle;
    FAIRequestID ActiveMoveId;
    FVector SearchCenter = FVector::ZeroVector;
    uint32 InvestigatedRevision = 0;
    float Radius = 160.0f;
    float Elapsed = 0.0f;
    float PauseRemaining = 0.0f;
    int32 CompletedSearchPoints = 0;
    int32 MoveAttempts = 0;
    bool bActive = false;
    bool bMovingToCenter = true;
    bool bMoveCompleted = false;
    bool bMoveSucceeded = false;
    bool bPaused = false;

    bool ReadLatestNoise(UBehaviorTreeComponent& OwnerComp);
    void StartMove(const FVector& Destination, bool bToCenter);
    bool StartNextSearchPoint(UBehaviorTreeComponent& OwnerComp);
    void HandleMoveFinished(FAIRequestID RequestId, const FPathFollowingResult& Result);
    void StopOwnedMove();
    void Cleanup();
    void FinishInvestigation(UBehaviorTreeComponent& OwnerComp, const TCHAR* Reason);
};
