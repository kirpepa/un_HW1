#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HW2AICharacter.generated.h"

class UBehaviorTree;

/** Guard pawn; its decisions and movement requests belong to its Behavior Tree. */
UCLASS()
class HW1_API AHW2AICharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AHW2AICharacter();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    TObjectPtr<UBehaviorTree> BehaviorTree;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Patrol", meta=(ClampMin="100", Units="cm"))
    float PatrolRadius = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Movement", meta=(ClampMin="1"))
    float PatrolSpeed = 250.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Movement", meta=(ClampMin="1"))
    float ChaseSpeed = 620.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI|Movement", meta=(ClampMin="1"))
    float SearchSpeed = 300.0f;
};
