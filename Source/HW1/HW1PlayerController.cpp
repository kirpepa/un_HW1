#include "HW1PlayerController.h"
#include "HW1.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Widgets/Input/SVirtualJoystick.h"

void AHW1PlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (IsLocalPlayerController() && ShouldUseTouchControls() && MobileControlsWidgetClass)
    {
        MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
        if (MobileControlsWidget)
        {
            MobileControlsWidget->AddToPlayerScreen(0);
        }
        else
        {
            UE_LOG(LogHW1, Error, TEXT("Could not create mobile controls."));
        }
    }
}

void AHW1PlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!IsLocalPlayerController())
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
    if (!Subsystem)
    {
        return;
    }

    for (UInputMappingContext* Context : DefaultMappingContexts)
    {
        if (Context)
        {
            Subsystem->AddMappingContext(Context, 0);
        }
    }
    if (!ShouldUseTouchControls())
    {
        for (UInputMappingContext* Context : MobileExcludedMappingContexts)
        {
            if (Context)
            {
                Subsystem->AddMappingContext(Context, 0);
            }
        }
    }
}

bool AHW1PlayerController::ShouldUseTouchControls() const
{
    return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}
