// BulletSystem: BulletLogicControllers.cpp
// Logic data assets and controllers.
#include "Logic/BulletLogicControllers.h"
#include "Logic/BulletLogicControllerTypes.h"
#include "Logic/BulletLogicBlueprintController.h"
#include "Logic/BulletLogicDataTypes.h"
#include "Controller/BulletController.h"
#include "Model/BulletInfo.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"

// Bind controller to runtime context (controller/entity/data).
void UBulletLogicController::Initialize(UBulletController* InController, UBulletEntity* InEntity, UBulletLogicData* InData)
{
    Controller = InController;
    Entity = InEntity;
    Data = InData;
}

// Create-bullet logic fires as a hit or begin trigger.
void UBulletLogicCreateBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

// Enqueue a SummonBullet action to keep creation in action pipeline.
void UBulletLogicCreateBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataCreateBullet* CreateData = Cast<UBulletLogicDataCreateBullet>(Data);
    if (!CreateData || !Controller)
    {
        return;
    }

    // Queue spawn as an action to keep creation in the action pipeline.
    FBulletActionInfo ActionInfo;
    ActionInfo.Type = EBulletActionType::SummonBullet;
    ActionInfo.ChildBulletID = CreateData->ChildBulletID;
    ActionInfo.SpawnCount = CreateData->Count;
    ActionInfo.SpreadAngle = CreateData->SpreadAngle;
    Controller->EnqueueAction(BulletInfo.BulletId, ActionInfo);
}

void UBulletLogicDestroyBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

// Request destroy via controller to keep lifecycle order.
void UBulletLogicDestroyBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataDestroyBullet* DestroyData = Cast<UBulletLogicDataDestroyBullet>(Data);
    if (!DestroyData || !Controller)
    {
        return;
    }

    Controller->RequestDestroyBullet(BulletInfo.BulletId, DestroyData->Reason, false);
}

// Apply an instantaneous force to velocity at begin.
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

// Freeze movement for a duration after hit.
void UBulletLogicFreezeController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataFreeze* FreezeData = Cast<UBulletLogicDataFreeze>(Data);
    if (!FreezeData || !Controller)
    {
        return;
    }

    BulletInfo.FrozenUntilTime = Controller->GetWorldTimeSeconds() + FreezeData->FreezeTime;
}

// Scale velocity on rebound.
void UBulletLogicReboundController::OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    const UBulletLogicDataRebound* ReboundData = Cast<UBulletLogicDataRebound>(Data);
    if (!ReboundData)
    {
        return;
    }

    BulletInfo.MoveInfo.Velocity *= ReboundData->ReboundFactor;
}

// Support response: tag bullet and scale velocity.
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

// Replace default movement with a curve-driven path.
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
        BulletInfo.MoveInfo.CustomMoveStartLocation = BulletInfo.MoveInfo.Location;
    }
    return true;
}

// Configure attach-to-owner when acting as a shield.
void UBulletLogicShieldController::OnBegin(FBulletInfo& BulletInfo)
{
    const UBulletLogicDataShield* ShieldData = Cast<UBulletLogicDataShield>(Data);
    if (!ShieldData)
    {
        return;
    }

    BulletInfo.MoveInfo.bAttachToOwner = ShieldData->bAttachToOwner;
}

// GAS hook: allow blueprint to apply GameplayEffect on hit.
void UBulletLogicApplyGEController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    UBulletLogicDataApplyGE* ApplyData = Cast<UBulletLogicDataApplyGE>(Data);
    if (!ApplyData || !Controller)
    {
        return;
    }

    if (ApplyData->bApplyOnServerOnly)
    {
        if (UWorld* World = Controller->GetWorld())
        {
            if (World->IsNetMode(NM_Client))
            {
                return;
            }
        }
    }

    if (!ShouldApplyEffect(BulletInfo, Hit))
    {
        return;
    }

    AActor* SourceActor = BulletInfo.InitParams.Owner;
    AActor* TargetActor = Hit.GetActor();
    if (!TargetActor)
    {
        TargetActor = BulletInfo.InitParams.TargetActor;
    }

    const bool bApplied = ApplyEffectBlueprint(ApplyData, SourceActor, TargetActor, Hit, ApplyData->EffectLevel);
    OnEffectApplied(BulletInfo, Hit, bApplied);
}

bool UBulletLogicApplyGEController::ShouldApplyEffect_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit) const
{
    (void)BulletInfo;
    (void)Hit;
    return true;
}

bool UBulletLogicApplyGEController::ApplyEffectBlueprint_Implementation(UBulletLogicDataApplyGE* ApplyData, AActor* SourceActor, AActor* TargetActor, const FHitResult& Hit, float EffectLevel) const
{
    (void)ApplyData;
    (void)SourceActor;
    (void)TargetActor;
    (void)Hit;
    (void)EffectLevel;
    return false;
}

void UBulletLogicApplyGEController::OnEffectApplied_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit, bool bSuccess)
{
    (void)BulletInfo;
    (void)Hit;
    (void)bSuccess;
}

// Blueprint extension hooks (K2_*) for custom logic.
void UBulletLogicBlueprintController::OnBegin(FBulletInfo& BulletInfo)
{
    K2_OnBegin(BulletInfo);
}

void UBulletLogicBlueprintController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    K2_OnHit(BulletInfo, Hit);
}

void UBulletLogicBlueprintController::OnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason)
{
    K2_OnDestroy(BulletInfo, Reason);
}

void UBulletLogicBlueprintController::OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    K2_OnRebound(BulletInfo, Hit);
}

void UBulletLogicBlueprintController::OnSupport(FBulletInfo& BulletInfo)
{
    K2_OnSupport(BulletInfo);
}

void UBulletLogicBlueprintController::OnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    K2_OnHitBullet(BulletInfo, OtherBulletId);
}

void UBulletLogicBlueprintController::Tick(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_Tick(BulletInfo, DeltaSeconds);
}

bool UBulletLogicBlueprintController::ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    return K2_ReplaceMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicBlueprintController::OnPreMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_OnPreMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicBlueprintController::OnPostMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_OnPostMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicBlueprintController::OnPreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    K2_OnPreCollision(BulletInfo, Hits);
}

void UBulletLogicBlueprintController::OnPostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    K2_OnPostCollision(BulletInfo, Hits);
}

bool UBulletLogicBlueprintController::FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    return K2_FilterHit(BulletInfo, Hit);
}

void UBulletLogicBlueprintController::K2_OnBegin_Implementation(FBulletInfo& BulletInfo)
{
    (void)BulletInfo;
}

void UBulletLogicBlueprintController::K2_OnHit_Implementation(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
}

void UBulletLogicBlueprintController::K2_OnDestroy_Implementation(FBulletInfo& BulletInfo, EBulletDestroyReason Reason)
{
    (void)BulletInfo;
    (void)Reason;
}

void UBulletLogicBlueprintController::K2_OnRebound_Implementation(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
}

void UBulletLogicBlueprintController::K2_OnSupport_Implementation(FBulletInfo& BulletInfo)
{
    (void)BulletInfo;
}

void UBulletLogicBlueprintController::K2_OnHitBullet_Implementation(FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    (void)BulletInfo;
    (void)OtherBulletId;
}

void UBulletLogicBlueprintController::K2_Tick_Implementation(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

bool UBulletLogicBlueprintController::K2_ReplaceMove_Implementation(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
    return false;
}

void UBulletLogicBlueprintController::K2_OnPreMove_Implementation(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

void UBulletLogicBlueprintController::K2_OnPostMove_Implementation(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

void UBulletLogicBlueprintController::K2_OnPreCollision_Implementation(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    (void)BulletInfo;
    (void)Hits;
}

void UBulletLogicBlueprintController::K2_OnPostCollision_Implementation(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    (void)BulletInfo;
    (void)Hits;
}

bool UBulletLogicBlueprintController::K2_FilterHit_Implementation(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
    return true;
}

