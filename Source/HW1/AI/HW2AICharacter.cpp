#include "AI/HW2AICharacter.h"
#include "AI/HW2AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

AHW2AICharacter::AHW2AICharacter()
{
    PrimaryActorTick.bCanEverTick = false;
    AIControllerClass = AHW2AIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 420.0f, 0.0f);
    GetCharacterMovement()->MaxWalkSpeed = PatrolSpeed;
    GetCharacterMovement()->bUseRVOAvoidance = true;
    GetCharacterMovement()->AvoidanceConsiderationRadius = 350.0f;
    GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -96.0f));
    GetMesh()->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GetMesh()->SetCanEverAffectNavigation(false);
    GetCapsuleComponent()->SetCanEverAffectNavigation(false);

    static ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (MeshAsset.Succeeded())
    {
        GetMesh()->SetSkeletalMesh(MeshAsset.Object);
    }
    static ConstructorHelpers::FClassFinder<UAnimInstance> AnimAsset(
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    if (AnimAsset.Succeeded())
    {
        GetMesh()->SetAnimInstanceClass(AnimAsset.Class);
    }
}
