#include "AI/HW2AIController.h"
#include "AI/HW2AICharacter.h"
#include "HW1.h"
#include "HW1Character.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

AHW2AIController::AHW2AIController()
{
    PrimaryActorTick.bCanEverTick = false;
    UAIPerceptionComponent* Senses = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("GuardPerception"));
    SetPerceptionComponent(*Senses);
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
    SightConfig->SightRadius = 1300.0f;
    SightConfig->LoseSightRadius = 1600.0f;
    SightConfig->PeripheralVisionAngleDegrees = 70.0f;
    SightConfig->SetMaxAge(2.0f);
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
    HearingConfig->HearingRange = 2400.0f;
    HearingConfig->SetMaxAge(4.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    Senses->ConfigureSense(*SightConfig);
    Senses->ConfigureSense(*HearingConfig);
    Senses->SetDominantSense(UAISense_Sight::StaticClass());
    Senses->OnTargetPerceptionUpdated.AddDynamic(this, &AHW2AIController::HandlePerceptionUpdated);
}

void AHW2AIController::GetActorEyesViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
    if (const APawn* Guard = GetPawn())
    {
        OutLocation = Guard->GetPawnViewLocation();
        // Sight follows the walking guard even though this controller has no decision Tick.
        OutRotation = Guard->GetActorRotation();
        return;
    }
    Super::GetActorEyesViewPoint(OutLocation, OutRotation);
}

void AHW2AIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    AHW2AICharacter* Guard = Cast<AHW2AICharacter>(InPawn);
    if (Guard && !Guard->BehaviorTree)
    {
        // Resolve at play time: the first editor authoring pass creates this asset.
        Guard->BehaviorTree = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/HW2/AI/BT_Guard.BT_Guard"));
    }
    if (!Guard || !Guard->BehaviorTree || !Guard->BehaviorTree->BlackboardAsset)
    {
        UE_LOG(LogHW1, Error, TEXT("HW2: %s needs BT_Guard with BB_Guard."), *GetNameSafe(InPawn));
        return;
    }
    UBlackboardComponent* Board = nullptr;
    if (!UseBlackboard(Guard->BehaviorTree->BlackboardAsset, Board) || !Board)
    {
        UE_LOG(LogHW1, Error, TEXT("HW2: failed to initialize guard Blackboard."));
        return;
    }
    NoiseRevision = 0;
    Board->SetValueAsVector(HW2Blackboard::HomeLocation, InPawn->GetActorLocation());
    Board->SetValueAsVector(HW2Blackboard::PatrolLocation, InPawn->GetActorLocation());
    Board->ClearValue(HW2Blackboard::TargetActor);
    Board->SetValueAsBool(HW2Blackboard::HasNoise, false);
    SetGuardState(EHW2GuardState::Patrol);
    if (!RunBehaviorTree(Guard->BehaviorTree))
    {
        UE_LOG(LogHW1, Error, TEXT("HW2: failed to run BT_Guard for %s."), *GetNameSafe(InPawn));
    }
}

void AHW2AIController::OnUnPossess()
{
    if (BrainComponent)
    {
        BrainComponent->StopLogic(TEXT("Guard unpossessed"));
    }
    StopMovement();
    Super::OnUnPossess();
}

void AHW2AIController::HandlePerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    const AHW1Character* Player = Cast<AHW1Character>(Actor);
    UBlackboardComponent* Board = GetBlackboardComponent();
    if (!Player || !Player->IsPlayerControlled() || !Board || !GetPawn())
    {
        return;
    }

    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            SetGuardState(EHW2GuardState::Chase);
            Board->SetValueAsBool(HW2Blackboard::HasNoise, false);
            Board->SetValueAsObject(HW2Blackboard::TargetActor, Actor);
        }
        else if (Board->GetValueAsObject(HW2Blackboard::TargetActor) == Actor)
        {
            // Explicit rubric behavior: loss of sight or excessive distance returns to patrol.
            Board->SetValueAsBool(HW2Blackboard::HasNoise, false);
            Board->ClearValue(HW2Blackboard::TargetActor);
            SetGuardState(EHW2GuardState::Patrol);
        }
        return;
    }

    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() &&
        Stimulus.WasSuccessfullySensed() && Stimulus.Tag == TEXT("Sprint") &&
        !Board->GetValueAsObject(HW2Blackboard::TargetActor))
    {
        const float Distance = FVector::Dist2D(GetPawn()->GetActorLocation(), Stimulus.StimulusLocation);
        const float Radius = FMath::GetMappedRangeValueClamped(
            FVector2D(0.0f, HearingConfig->HearingRange), FVector2D(160.0f, 650.0f), Distance);
        ++NoiseRevision;
        Board->SetValueAsVector(HW2Blackboard::NoiseLocation, Stimulus.StimulusLocation);
        Board->SetValueAsFloat(HW2Blackboard::SearchRadius, Radius);
        // Set the decorator condition last, after the task's input has been written.
        Board->SetValueAsBool(HW2Blackboard::HasNoise, true);
        UE_LOG(LogHW1, Log, TEXT("HW2: %s heard sprint; distance=%.0f search radius=%.0f revision=%u"),
            *GetNameSafe(GetPawn()), Distance, Radius, NoiseRevision);
    }
}

void AHW2AIController::ConsumeNoise(uint32 InvestigatedRevision)
{
    if (InvestigatedRevision == NoiseRevision && GetBlackboardComponent())
    {
        GetBlackboardComponent()->SetValueAsBool(HW2Blackboard::HasNoise, false);
    }
}

void AHW2AIController::SetGuardState(EHW2GuardState NewState)
{
    const bool bChanged = GuardState != NewState;
    GuardState = NewState;
    if (AHW2AICharacter* Guard = Cast<AHW2AICharacter>(GetPawn()))
    {
        Guard->GetCharacterMovement()->MaxWalkSpeed = NewState == EHW2GuardState::Chase
            ? Guard->ChaseSpeed : NewState == EHW2GuardState::Investigate ? Guard->SearchSpeed : Guard->PatrolSpeed;
    }
    if (bChanged)
    {
        UE_LOG(LogHW1, Log, TEXT("HW2: %s state=%s"), *GetNameSafe(GetPawn()), *GetGuardStateLabel());
    }
}

FString AHW2AIController::GetGuardStateLabel() const
{
    switch (GuardState)
    {
        case EHW2GuardState::Chase: return TEXT("CHASE");
        case EHW2GuardState::Investigate: return TEXT("INVESTIGATE");
        default: return TEXT("PATROL");
    }
}
