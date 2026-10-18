#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HW1Character.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteraction);

/** Third-person movement, sprint/stamina, and a Blueprint interaction dispatcher. */
UCLASS()
class HW1_API AHW1Character : public ACharacter
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
    UCameraComponent* FollowCamera;

protected:
    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* JumpAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, Category="Input")
    UInputAction* MouseLookAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    UInputAction* SprintAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
    UInputAction* InteractAction;

    virtual void BeginPlay() override;
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

public:
    AHW1Character();
    virtual void Tick(float DeltaSeconds) override;
    virtual void UnPossessed() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FOnInteraction OnInteraction;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Sprint")
    bool bIsSprinting = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint", meta=(ClampMin="0"))
    float WalkSpeed = 500.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sprint", meta=(ClampMin="0"))
    float SprintSpeed = 900.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(ClampMin="1"))
    float MaxStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(ClampMin="0"))
    float StaminaDrainRate = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Stamina", meta=(ClampMin="0"))
    float StaminaRegenerationRate = 20.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Stamina")
    float Stamina = 100.0f;

    UFUNCTION(BlueprintCallable, Category="Sprint")
    void BeginSprint();

    UFUNCTION(BlueprintCallable, Category="Sprint")
    void EndSprint();

    UFUNCTION(BlueprintCallable, Category="Interaction")
    void Interact();

    UFUNCTION(BlueprintPure, Category="Stamina")
    float GetStaminaFraction() const;

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoMove(float Right, float Forward);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoLook(float Yaw, float Pitch);

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpStart();

    UFUNCTION(BlueprintCallable, Category="Input")
    virtual void DoJumpEnd();

    USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
    UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:
    // Exhaustion deliberately keeps this latched until the input is released.
    bool bSprintInputHeld = false;
    void StopSprinting();
};
