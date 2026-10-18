#include "HW1Character.h"
#include "HW1.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

AHW1Character::AHW1Character()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
    GetCharacterMovement()->JumpZVelocity = 500.0f;
    GetCharacterMovement()->AirControl = 0.35f;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
    GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
    GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
    GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 400.0f;
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
}

void AHW1Character::BeginPlay()
{
    Super::BeginPlay();
    Stamina = FMath::Max(0.0f, MaxStamina);
    EndSprint();
}

void AHW1Character::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Step = FMath::Max(0.0f, DeltaSeconds);
    const float Capacity = FMath::Max(0.0f, MaxStamina);

    if (bIsSprinting)
    {
        Stamina = FMath::Clamp(Stamina - FMath::Max(0.0f, StaminaDrainRate) * Step, 0.0f, Capacity);
        if (Stamina <= 0.0f)
        {
            StopSprinting();
        }
    }
    else
    {
        Stamina = FMath::Clamp(Stamina + FMath::Max(0.0f, StaminaRegenerationRate) * Step, 0.0f, Capacity);
    }
}

void AHW1Character::BeginSprint()
{
    if (bSprintInputHeld)
    {
        return;
    }
    bSprintInputHeld = true;
    if (Stamina > 0.0f)
    {
        bIsSprinting = true;
        GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
    }
}

void AHW1Character::StopSprinting()
{
    bIsSprinting = false;
    GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void AHW1Character::EndSprint()
{
    bSprintInputHeld = false;
    StopSprinting();
}

void AHW1Character::UnPossessed()
{
    EndSprint();
    Super::UnPossessed();
}

void AHW1Character::Interact()
{
    OnInteraction.Broadcast();
}

float AHW1Character::GetStaminaFraction() const
{
    return MaxStamina > 0.0f ? FMath::Clamp(Stamina / MaxStamina, 0.0f, 1.0f) : 0.0f;
}

void AHW1Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        UE_LOG(LogHW1, Error, TEXT("%s requires an Enhanced Input component."), *GetNameSafe(this));
        return;
    }

    if (JumpAction)
    {
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &AHW1Character::DoJumpStart);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &AHW1Character::DoJumpEnd);
        EnhancedInput->BindAction(JumpAction, ETriggerEvent::Canceled, this, &AHW1Character::DoJumpEnd);
    }
    if (MoveAction)
    {
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHW1Character::Move);
    }
    if (LookAction)
    {
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHW1Character::Look);
    }
    if (MouseLookAction)
    {
        EnhancedInput->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AHW1Character::Look);
    }
    if (SprintAction)
    {
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Started, this, &AHW1Character::BeginSprint);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHW1Character::EndSprint);
        EnhancedInput->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AHW1Character::EndSprint);
    }
    if (InteractAction)
    {
        EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AHW1Character::Interact);
    }
}

void AHW1Character::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    DoMove(Axis.X, Axis.Y);
}

void AHW1Character::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    DoLook(Axis.X, Axis.Y);
}

void AHW1Character::DoMove(float Right, float Forward)
{
    if (GetController())
    {
        const FRotator YawRotation(0.0f, GetController()->GetControlRotation().Yaw, 0.0f);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Forward);
        AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Right);
    }
}

void AHW1Character::DoLook(float Yaw, float Pitch)
{
    if (GetController())
    {
        AddControllerYawInput(Yaw);
        AddControllerPitchInput(Pitch);
    }
}

void AHW1Character::DoJumpStart()
{
    Jump();
}

void AHW1Character::DoJumpEnd()
{
    StopJumping();
}
