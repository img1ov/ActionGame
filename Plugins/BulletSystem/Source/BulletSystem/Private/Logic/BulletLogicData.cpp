#include "Logic/BulletLogicData.h"
#include "Logic/BulletLogicControllers.h"

UBulletLogicDataCreateBullet::UBulletLogicDataCreateBullet()
{
    Trigger = EBulletLogicTrigger::OnHit;
    ControllerClass = UBulletLogicCreateBulletController::StaticClass();
}

UBulletLogicDataDestroyBullet::UBulletLogicDataDestroyBullet()
{
    Trigger = EBulletLogicTrigger::OnHit;
    ControllerClass = UBulletLogicDestroyBulletController::StaticClass();
}

UBulletLogicDataForce::UBulletLogicDataForce()
{
    Trigger = EBulletLogicTrigger::OnBegin;
    ControllerClass = UBulletLogicForceController::StaticClass();
}

UBulletLogicDataFreeze::UBulletLogicDataFreeze()
{
    Trigger = EBulletLogicTrigger::OnHit;
    ControllerClass = UBulletLogicFreezeController::StaticClass();
}

UBulletLogicDataRebound::UBulletLogicDataRebound()
{
    Trigger = EBulletLogicTrigger::OnRebound;
    ControllerClass = UBulletLogicReboundController::StaticClass();
}

UBulletLogicDataSupport::UBulletLogicDataSupport()
{
    Trigger = EBulletLogicTrigger::OnSupport;
    ControllerClass = UBulletLogicSupportController::StaticClass();
}

UBulletLogicDataCurveMovement::UBulletLogicDataCurveMovement()
{
    Trigger = EBulletLogicTrigger::ReplaceMove;
    ControllerClass = UBulletLogicCurveMovementController::StaticClass();
}

UBulletLogicDataShield::UBulletLogicDataShield()
{
    Trigger = EBulletLogicTrigger::OnBegin;
    ControllerClass = UBulletLogicShieldController::StaticClass();
}
