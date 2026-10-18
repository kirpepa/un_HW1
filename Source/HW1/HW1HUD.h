#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HW1HUD.generated.h"

UCLASS()
class HW1_API AHW1HUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};
