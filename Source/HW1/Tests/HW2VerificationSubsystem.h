#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HW2VerificationSubsystem.generated.h"

class AHW1Character;
class AHW2AICharacter;
class AHW2AIController;

/** Opt-in integration checks in the real game world; absent from normal gameplay. */
UCLASS()
class UHW2VerificationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(HW2Verification, STATGROUP_Tickables); }
private:
    void Advance(int32 Next);
    bool Require(bool Condition, const TCHAR* Message);
    void Finish(bool bSuccess);
    bool PlacePlayer(float Distance, bool bRequireVisibility);
    void MovePlayerFarAway();
    TWeakObjectPtr<AHW1Character> Player;
    TWeakObjectPtr<AHW2AICharacter> Guard;
    TWeakObjectPtr<AHW2AIController> Controller;
    float Elapsed = 0;
    float PhaseTime = 0;
    int32 Phase = 0;
    int32 NoiseRevision = 0;
    FVector StartLocation = FVector::ZeroVector;
    bool bFinished = false;
};
