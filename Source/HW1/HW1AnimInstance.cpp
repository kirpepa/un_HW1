#include "HW1AnimInstance.h"
#include "HW1Character.h"
#include "AI/HW2AICharacter.h"

void UHW1AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const APawn* Owner = TryGetPawnOwner();
    if (const AHW1Character* Character = Cast<AHW1Character>(Owner))
    {
        bIsSprinting = Character->bIsSprinting;
    }
    else if (const AHW2AICharacter* Guard = Cast<AHW2AICharacter>(Owner))
    {
        bIsSprinting = Guard->GetVelocity().SizeSquared2D() > FMath::Square(Guard->PatrolSpeed + 100.0f);
    }
    else
    {
        bIsSprinting = false;
    }
}
