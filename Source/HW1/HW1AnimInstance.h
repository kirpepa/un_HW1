#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HW1AnimInstance.generated.h"

/** Native bridge to the locomotion animation Blueprint. */
UCLASS(Transient, Blueprintable)
class HW1_API UHW1AnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
    bool bIsSprinting = false;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
