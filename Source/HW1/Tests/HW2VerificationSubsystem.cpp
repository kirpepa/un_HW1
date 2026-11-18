#include "Tests/HW2VerificationSubsystem.h"
#include "AI/HW2AICharacter.h"
#include "AI/HW2AIController.h"
#include "HW1Character.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BrainComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "Perception/AISense_Hearing.h"
#include "PCGComponent.h"

bool UHW2VerificationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return !UE_BUILD_SHIPPING && World && World->IsGameWorld()
        && FParse::Param(FCommandLine::Get(), TEXT("HW2Verify"))
        && Super::ShouldCreateSubsystem(Outer);
}

void UHW2VerificationSubsystem::Advance(int32 Next)
{
    Phase = Next;
    PhaseTime = 0;
    UE_LOG(LogTemp, Display, TEXT("HW2 VERIFY: phase %d"), Phase);
}

bool UHW2VerificationSubsystem::Require(bool Condition, const TCHAR* Message)
{
    UE_LOG(LogTemp, Display, TEXT("HW2 VERIFY: %s: %s"), Condition ? TEXT("PASS") : TEXT("FAIL"), Message);
    if (!Condition) Finish(false);
    return Condition;
}

void UHW2VerificationSubsystem::Finish(bool bSuccess)
{
    if (bFinished) return;
    bFinished = true;
    UE_LOG(LogTemp, Display, TEXT("HW2 VERIFY RESULT: %s (%.1f seconds)"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), Elapsed);
    FFileHelper::SaveStringToFile(bSuccess ? TEXT("PASS\n") : TEXT("FAIL\n"),
        *(FPaths::ProjectSavedDir() / TEXT("HW2Verification.txt")));
    // macOS's generic RequestExitWithStatus ignores its status; the explicit report is authoritative.
    FPlatformMisc::RequestExit(false);
}

void UHW2VerificationSubsystem::MovePlayerFarAway()
{
    const FVector G = Guard->GetActorLocation();
    Player->GetCharacterMovement()->StopMovementImmediately();
    Player->SetActorLocation(FVector(G.X > 0 ? -2900 : 2900, G.Y > 0 ? -2500 : 2500, 100), false, nullptr, ETeleportType::TeleportPhysics);
}

bool UHW2VerificationSubsystem::PlacePlayer(float Distance, bool bRequireVisibility)
{
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const FVector Origin = Guard->GetActorLocation();
    for (int32 Index = 0; Index < 16; ++Index)
    {
        const float Angle = Index * 2 * PI / 16;
        const FVector Candidate = Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Distance;
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(120, 120, 250))) continue;
        const FVector Location = Projected.Location + FVector(0, 0, 98);
        if (FVector::Dist2D(Location, Origin) < Distance * .9f) continue;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(HW2VerifyVisibility), false, Guard.Get());
        Params.AddIgnoredActor(Player.Get());
        FHitResult Hit;
        if (bRequireVisibility && GetWorld()->LineTraceSingleByChannel(Hit, Origin + FVector(0, 0, 50), Location + FVector(0, 0, 50), ECC_Visibility, Params)) continue;
        Player->GetCharacterMovement()->StopMovementImmediately();
        Player->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
        if (bRequireVisibility)
        {
            const FRotator Facing = (Location - Origin).Rotation();
            Guard->SetActorRotation(Facing);
            Controller->SetControlRotation(Facing);
        }
        return true;
    }
    return false;
}

void UHW2VerificationSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (bFinished || !GetWorld()->HasBegunPlay()) return;
    Elapsed += DeltaTime;
    PhaseTime += DeltaTime;
    if (Elapsed > 90) { Require(false, TEXT("global integration timeout")); return; }

    if (Phase == 0)
    {
        Player = Cast<AHW1Character>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
        TActorIterator<AHW2AICharacter> GuardIt(GetWorld());
        if (GuardIt) Guard = *GuardIt;
        if (!Player.IsValid() || !Guard.IsValid() || PhaseTime < 3) return;
        Controller = Cast<AHW2AIController>(Guard->GetController());
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
        FNavLocation Point;
        if (!Nav || !Nav->ProjectPointToNavigation(Guard->GetActorLocation(), Point)) return;
        if (!Require(Controller.IsValid() && Controller->GetBlackboardComponent() && Controller->GetBrainComponent()
            && Controller->GetBrainComponent()->IsRunning(), TEXT("guard possesses pawn and runs Behavior Tree"))) return;
        int32 Instances = 0;
        bool bCollidingPCG = false;
        bool bNavigationAvoidsCover = false;
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (!It->FindComponentByClass<UPCGComponent>()) continue;
            TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
            for (auto* Mesh : Components)
            {
                Instances += Mesh->GetInstanceCount();
                bCollidingPCG |= Mesh->GetInstanceCount() > 0 && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision && Mesh->CanEverAffectNavigation();
                if (Mesh->GetInstanceCount() > 0)
                {
                    FTransform Instance;
                    Mesh->GetInstanceTransform(0, Instance, true);
                    const FVector Center = Instance.GetLocation();
                    FNavLocation InsideCover;
                    const bool bInsideProjects = Nav->ProjectPointToNavigation(FVector(Center.X, Center.Y, 5), InsideCover, FVector(10, 10, 30));
                    UE_LOG(LogTemp, Display, TEXT("HW2 VERIFY cover: center=%s scale=%s projected=%d location=%s collision=%d nav=%d"),
                        *Center.ToString(), *Instance.GetScale3D().ToString(), bInsideProjects,
                        *InsideCover.Location.ToString(), int32(Mesh->GetCollisionEnabled()), Mesh->CanEverAffectNavigation());
                    bNavigationAvoidsCover |= !bInsideProjects;
                }
            }
        }
        if (!Require(Instances >= 20 && bCollidingPCG, TEXT("PCG generated at least 20 colliding navigation obstacles"))) return;
        if (!Require(bNavigationAvoidsCover, TEXT("NavMesh excludes ground inside PCG obstacles"))) return;
        MovePlayerFarAway();
        StartLocation = Guard->GetActorLocation();
        Advance(1);
        return;
    }

    if (!Player.IsValid() || !Guard.IsValid() || !Controller.IsValid())
    {
        Require(false, TEXT("test actors remain alive"));
        return;
    }
    auto* BB = Controller->GetBlackboardComponent();
    switch (Phase)
    {
    case 1:
        if (PhaseTime < 4) break;
        if (!Require(FVector::Dist2D(StartLocation, Guard->GetActorLocation()) > 80, TEXT("patrol actually moves on generated map"))) return;
        if (!Require(FVector::Dist2D(BB->GetValueAsVector(TEXT("HomeLocation")), BB->GetValueAsVector(TEXT("PatrolLocation"))) <= 610, TEXT("patrol destination remains within home radius"))) return;
        if (!Require(PlacePlayer(1950, false), TEXT("reachable hearing-only position exists"))) return;
        NoiseRevision = Controller->GetNoiseRevision();
        Player->BeginSprint();
        Advance(2);
        break;
    case 2:
        if (PhaseTime < 1) break;
        if (!Require(Controller->GetNoiseRevision() == NoiseRevision, TEXT("stationary sprint does not emit footsteps"))) return;
        Player->EndSprint();
        Player->BeginSprint();
        Advance(3);
        break;
    case 3:
    {
        const FVector ToGuard = (Guard->GetActorLocation() - Player->GetActorLocation()).GetSafeNormal2D();
        Player->AddMovementInput(FVector(ToGuard.Y, -ToGuard.X, 0));
        if (Controller->GetNoiseRevision() > NoiseRevision && BB->GetValueAsBool(TEXT("HasNoise")))
        {
            Require(true, TEXT("moving sprint is heard and activates investigation"));
            Player->EndSprint();
            MovePlayerFarAway();
            StartLocation = Guard->GetActorLocation();
            Advance(4);
        }
        else if (PhaseTime > 4) Require(false, TEXT("moving sprint must trigger hearing within 4 seconds"));
        break;
    }
    case 4:
        if (PhaseTime > 1 && !BB->GetValueAsBool(TEXT("HasNoise")))
        {
            if (!Require(FVector::Dist2D(StartLocation, Guard->GetActorLocation()) > 100, TEXT("investigation moves to noise and completes"))) return;
            if (!Require(BB->GetValueAsObject(TEXT("TargetActor")) == nullptr, TEXT("hearing alone never grants visual target"))) return;
            if (!Require(PlacePlayer(1950, false), TEXT("second investigation setup"))) return;
            NoiseRevision = Controller->GetNoiseRevision();
            UAISense_Hearing::ReportNoiseEvent(GetWorld(), Player->GetActorLocation(), 1, Player.Get(), 2400, TEXT("Sprint"));
            Advance(5);
        }
        else if (PhaseTime > 26) Require(false, TEXT("investigation must finish within its time budget"));
        break;
    case 5:
        if (Controller->GetNoiseRevision() > NoiseRevision && BB->GetValueAsBool(TEXT("HasNoise")))
        {
            if (!Require(PlacePlayer(700, true), TEXT("visible target position for search interruption"))) return;
            StartLocation = Guard->GetActorLocation();
            Advance(6);
        }
        else if (PhaseTime > 4) Require(false, TEXT("guard accepts another noise after search completion"));
        break;
    case 6:
        if (BB->GetValueAsObject(TEXT("TargetActor")) == Player.Get())
        {
            if (!Require(!BB->GetValueAsBool(TEXT("HasNoise")), TEXT("sight interrupts and cancels investigation"))) return;
            Advance(7);
        }
        else if (PhaseTime > 4) Require(false, TEXT("sight must detect visible player"));
        break;
    case 7:
        if (PhaseTime < 1.5f) break;
        if (!Require(FVector::Dist2D(StartLocation, Guard->GetActorLocation()) > 100, TEXT("chase moves toward player through Behavior Tree"))) return;
        MovePlayerFarAway();
        Advance(8);
        break;
    case 8:
        if (PhaseTime > .5f && !BB->GetValueAsObject(TEXT("TargetActor")))
        {
            if (!Require(!BB->GetValueAsBool(TEXT("HasNoise")), TEXT("escaping sight clears chase and returns to patrol"))) return;
            StartLocation = Guard->GetActorLocation();
            Advance(9);
        }
        else if (PhaseTime > 5) Require(false, TEXT("escaping player must be forgotten"));
        break;
    case 9:
        if (PhaseTime < 4) break;
        if (!Require(FVector::Dist2D(StartLocation, Guard->GetActorLocation()) > 80
            && FVector::Dist2D(BB->GetValueAsVector(TEXT("HomeLocation")), BB->GetValueAsVector(TEXT("PatrolLocation"))) <= 610,
            TEXT("patrol actually resumes toward its original home area"))) return;
        Finish(true);
        break;
    default: break;
    }
}
