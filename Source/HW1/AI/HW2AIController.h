#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "HW2AIController.generated.h"

class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;

UENUM(BlueprintType)
enum class EHW2GuardState : uint8
{
    Patrol,
    Investigate,
    Chase
};

/** Blackboard contract shared by the controller, custom tasks, and BT_Guard. */
namespace HW2Blackboard
{
    inline const FName HomeLocation(TEXT("HomeLocation"));
    inline const FName PatrolLocation(TEXT("PatrolLocation"));
    inline const FName TargetActor(TEXT("TargetActor"));
    inline const FName HasNoise(TEXT("HasNoise"));
    inline const FName NoiseLocation(TEXT("NoiseLocation"));
    inline const FName SearchRadius(TEXT("SearchRadius"));
}

/** Event-driven perception supplies facts; the Behavior Tree chooses actions. */
UCLASS()
class HW1_API AHW2AIController : public AAIController
{
    GENERATED_BODY()

public:
    AHW2AIController();
    virtual void GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const override;

    UFUNCTION(BlueprintPure, Category="AI|Debug")
    EHW2GuardState GetGuardState() const { return GuardState; }

    UFUNCTION(BlueprintPure, Category="AI|Debug")
    FString GetGuardStateLabel() const;

    void SetGuardState(EHW2GuardState NewState);
    uint32 GetNoiseRevision() const { return NoiseRevision; }

    /** A finishing search must not erase a newer sound. */
    void ConsumeNoise(uint32 InvestigatedRevision);

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception")
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI|Perception")
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

private:
    UFUNCTION()
    void HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(VisibleInstanceOnly, Category="AI|Debug")
    EHW2GuardState GuardState = EHW2GuardState::Patrol;

    uint32 NoiseRevision = 0;
};
