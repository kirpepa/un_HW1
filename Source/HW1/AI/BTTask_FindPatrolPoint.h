#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindPatrolPoint.generated.h"

/** Selects a reachable point around the ORIGINAL spawn, never around the current pawn. */
UCLASS()
class HW1_API UBTTask_FindPatrolPoint : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_FindPatrolPoint();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual FString GetStaticDescription() const override;
};
