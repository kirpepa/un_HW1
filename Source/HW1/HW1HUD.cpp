#include "HW1HUD.h"
#include "HW1Character.h"
#include "AI/HW2AICharacter.h"
#include "AI/HW2AIController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void AHW1HUD::DrawHUD()
{
    Super::DrawHUD();
    const AHW1Character* Character = Cast<AHW1Character>(GetOwningPawn());
    if (!Canvas || !Character)
    {
        return;
    }

    const float Scale = FMath::Clamp(Canvas->ClipY / 720.0f, 0.7f, 2.0f);
    for (TActorIterator<AHW2AICharacter> It(GetWorld()); It; ++It)
    {
        const AHW2AIController* Guard = Cast<AHW2AIController>(It->GetController());
        if (!Guard) continue;
        const EHW2GuardState State = Guard->GetGuardState();
        const FLinearColor Color = State == EHW2GuardState::Chase
            ? FLinearColor(1, .3f, .2f) : State == EHW2GuardState::Investigate
            ? FLinearColor(1, .75f, .2f) : FLinearColor(.2f, .85f, .7f);
        DrawRect(FLinearColor(.025f, .035f, .05f, .88f), 16 * Scale, 16 * Scale, 390 * Scale, 108 * Scale);
        DrawText(TEXT("SILENT COURTYARD"), FLinearColor::White, 28 * Scale, 26 * Scale, GEngine->GetMediumFont(), Scale);
        DrawText(FString::Printf(TEXT("GUARD  %s   |   %.0f m"), *Guard->GetGuardStateLabel(),
            FVector::Dist2D(Character->GetActorLocation(), It->GetActorLocation()) / 100),
            Color, 28 * Scale, 57 * Scale, GEngine->GetSmallFont(), Scale);
        DrawText(TEXT("Sprint nearby to make noise. Use cover to stay unseen."),
            FLinearColor(.78f, .84f, .9f), 28 * Scale, 86 * Scale, GEngine->GetSmallFont(), Scale);
        break;
    }
    const float X = 28.0f * Scale;
    const float Y = Canvas->ClipY - 136.0f * Scale;
    const float Width = 296.0f * Scale;
    const float Height = 16.0f * Scale;
    const float Fraction = Character->GetStaminaFraction();
    const FLinearColor FillColor = Fraction < 0.25f
        ? FLinearColor(1.0f, 0.35f, 0.2f)
        : FLinearColor(0.15f, 0.85f, 0.65f);

    DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.88f), X - 12.0f * Scale,
        Y - 12.0f * Scale, Width + 24.0f * Scale, 112.0f * Scale);
    DrawText(FString::Printf(TEXT("STAMINA  %.0f / %.0f%s"), Character->Stamina,
        Character->MaxStamina, Character->bIsSprinting ? TEXT("  |  SPRINT") : TEXT("")),
        FLinearColor::White, X, Y, GEngine->GetSmallFont(), Scale);
    DrawRect(FLinearColor(0.16f, 0.19f, 0.23f), X, Y + 26.0f * Scale, Width, Height);
    DrawRect(FillColor, X + 2.0f * Scale, Y + 28.0f * Scale,
        (Width - 4.0f * Scale) * Fraction, Height - 4.0f * Scale);
    DrawText(TEXT("WASD Move   Mouse Look   Space Jump"), FLinearColor(0.78f, 0.84f, 0.9f),
        X, Y + 52.0f * Scale, GEngine->GetSmallFont(), Scale);
    DrawText(TEXT("Shift Sprint (hold)   E Interact"), FLinearColor(0.78f, 0.84f, 0.9f),
        X, Y + 72.0f * Scale, GEngine->GetSmallFont(), Scale);
}
