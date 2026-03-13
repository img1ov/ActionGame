#include "Logic/BulletLogicControllers.h"
#include "Logic/BulletLogicData.h"
#include "Controller/BulletController.h"
#include "Model/BulletInfo.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"

void UBulletLogicController::Initialize(UBulletController* InController, UBulletEntity* InEntity, UBulletLogicData* InData)
{
    Controller = InController;
    Entity = InEntity;
    Data = InData;
}

void UBulletLogicCreateBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

void UBulletLogicCreateBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataCreateBullet* CreateData = Cast<UBulletLogicDataCreateBullet>(Data);
    if (!CreateData || !Controller)
    {
        return;
    }

    Controller->SpawnChildBulletsFromLogic(BulletInfo, CreateData->ChildRowName, CreateData->Count, CreateData->SpreadAngle);
}

void UBulletLogicDestroyBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

void UBulletLogicDestroyBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataDestroyBullet* DestroyData = Cast<UBulletLogicDataDestroyBullet>(Data);
    if (!DestroyData || !Controller)
    {
        return;
    }

    Controller->RequestDestroyBullet(BulletInfo.BulletId, DestroyData->Reason, false);
}

void UBulletLogicForceController::OnBegin(FBulletInfo& BulletInfo)
{
    const UBulletLogicDataForce* ForceData = Cast<UBulletLogicDataForce>(Data);
    if (!ForceData)
    {
        return;
    }

    FVector Force = ForceData->Force;
    if (ForceData->bForwardSpace)
    {
        Force = BulletInfo.MoveInfo.Rotation.RotateVector(Force);
    }

    if (ForceData->bAdditive)
    {
        BulletInfo.MoveInfo.Velocity += Force;
    }
    else
    {
        BulletInfo.MoveInfo.Velocity = Force;
    }
}

void UBulletLogicFreezeController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataFreeze* FreezeData = Cast<UBulletLogicDataFreeze>(Data);
    if (!FreezeData || !Controller)
    {
        return;
    }

    BulletInfo.FrozenUntilTime = Controller->GetWorldTimeSeconds() + FreezeData->FreezeTime;
}

void UBulletLogicReboundController::OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataRebound* ReboundData = Cast<UBulletLogicDataRebound>(Data);
    if (!ReboundData)
    {
        return;
    }

    BulletInfo.MoveInfo.Velocity *= ReboundData->ReboundFactor;
}

void UBulletLogicSupportController::OnSupport(FBulletInfo& BulletInfo)
{
    const UBulletLogicDataSupport* SupportData = Cast<UBulletLogicDataSupport>(Data);
    if (!SupportData)
    {
        return;
    }

    BulletInfo.Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Bullet.Support"), false));
    BulletInfo.MoveInfo.Velocity *= SupportData->DamageMultiplier;
}

bool UBulletLogicCurveMovementController::ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)DeltaSeconds;

    const UBulletLogicDataCurveMovement* CurveData = Cast<UBulletLogicDataCurveMovement>(Data);
    if (!CurveData || !CurveData->Curve)
    {
        return false;
    }

    if (BulletInfo.MoveInfo.CustomMoveCurve != CurveData->Curve)
    {
        BulletInfo.MoveInfo.CustomMoveCurve = CurveData->Curve;
        BulletInfo.MoveInfo.CustomMoveDuration = CurveData->Duration;
        BulletInfo.MoveInfo.CustomMoveElapsed = 0.0f;
    }
    return true;
}

void UBulletLogicShieldController::OnBegin(FBulletInfo& BulletInfo)
{
    const UBulletLogicDataShield* ShieldData = Cast<UBulletLogicDataShield>(Data);
    if (!ShieldData)
    {
        return;
    }

    BulletInfo.MoveInfo.bAttachToOwner = ShieldData->bAttachToOwner;
}
