#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HW1PlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/** Adds the template's mapping contexts for the local player. */
UCLASS()
class HW1_API AHW1PlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
    UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
    TArray<UInputMappingContext*> DefaultMappingContexts;

    UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
    TArray<UInputMappingContext*> MobileExcludedMappingContexts;

    UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
    TSubclassOf<UUserWidget> MobileControlsWidgetClass;

    UPROPERTY()
    TObjectPtr<UUserWidget> MobileControlsWidget;

    UPROPERTY(EditAnywhere, Config, Category="Input|Touch Controls")
    bool bForceTouchControls = false;

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    bool ShouldUseTouchControls() const;
};
