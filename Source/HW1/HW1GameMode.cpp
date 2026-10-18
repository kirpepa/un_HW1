#include "HW1GameMode.h"
#include "HW1Character.h"
#include "HW1HUD.h"
#include "HW1PlayerController.h"

AHW1GameMode::AHW1GameMode()
{
    DefaultPawnClass = AHW1Character::StaticClass();
    PlayerControllerClass = AHW1PlayerController::StaticClass();
    HUDClass = AHW1HUD::StaticClass();
}
