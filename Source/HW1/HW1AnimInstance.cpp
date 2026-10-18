#include "HW1AnimInstance.h"
#include "HW1Character.h"

void UHW1AnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const AHW1Character* Character = Cast<AHW1Character>(TryGetPawnOwner());
    bIsSprinting = Character && Character->bIsSprinting;
}
