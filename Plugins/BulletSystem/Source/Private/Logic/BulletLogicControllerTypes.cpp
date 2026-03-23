// BulletSystem: BulletLogicControllerTypes.cpp
// Controller implementations for typed LogicControllers.

#include "Logic/BulletLogicControllerTypes.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Actor/BulletActor.h"
#include "Controller/BulletController.h"
#include "Entity/BulletEntity.h"
#include "Model/BulletInfo.h"

class UAbilitySystemComponent;

void UBulletLogicCreateBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

void UBulletLogicCreateBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)Hit;

    const UBulletLogicDataCreateBullet* CreateData = Cast<UBulletLogicDataCreateBullet>(Data);
    if (!CreateData || !Controller)
    {
        return;
    }

    FBulletActionInfo ActionInfo;
    ActionInfo.Type = EBulletActionType::SummonBullet;
    ActionInfo.ChildBulletId = CreateData->ChildBulletId;
    ActionInfo.SpawnCount = CreateData->Count;
    ActionInfo.SpreadAngle = CreateData->SpreadAngle;
    Controller->EnqueueAction(BulletInfo.InstanceId, ActionInfo);
}

void UBulletLogicDestroyBulletController::OnBegin(FBulletInfo& BulletInfo)
{
    OnHit(BulletInfo, FHitResult());
}

void UBulletLogicDestroyBulletController::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)Hit;

    const UBulletLogicDataDestroyBullet* DestroyData = Cast<UBulletLogicDataDestroyBullet>(Data);
    if (!DestroyData || !Controller)
    {
        return;
    }

    Controller->RequestDestroyBullet(BulletInfo.InstanceId);
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
    (void)Hit;

    const UBulletLogicDataFreeze* FreezeData = Cast<UBulletLogicDataFreeze>(Data);
    if (!FreezeData || !Controller)
    {
        return;
    }

    BulletInfo.FrozenUntilTime = Controller->GetWorldTimeSeconds() + FreezeData->FreezeTime;
}

void UBulletLogicReboundController::OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)Hit;

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
        BulletInfo.MoveInfo.CustomMoveStartLocation = BulletInfo.MoveInfo.Location;
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

bool UBulletLogicController_ApplyGameplayEffect::ShouldSkipByNetMode(const UBulletLogicData_ApplyGameplayEffect* ApplyData) const
{
    if (!ApplyData || !Controller)
    {
        return true;
    }

    if (!ApplyData->bApplyOnServerOnly)
    {
        return false;
    }

    if (UWorld* World = Controller->GetWorld())
    {
        return World->IsNetMode(NM_Client);
    }

    return false;
}

bool UBulletLogicController_ApplyGameplayEffect::ApplyEffectToTarget(
    UBulletLogicData_ApplyGameplayEffect* ApplyData,
    const FBulletInfo& BulletInfo,
    AActor* SourceActor,
    AActor* TargetActor,
    const FHitResult& TargetHit) const
{
    if (!ApplyData || !SourceActor || !TargetActor)
    {
        return false;
    }

    // Blueprint override path.
    if (K2_ApplyEffect(ApplyData, SourceActor, TargetActor, TargetHit, ApplyData->EffectLevel))
    {
        return true;
    }

    if (!ApplyData->GameplayEffect)
    {
        return false;
    }

    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(SourceActor);
    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
    if (!SourceASC || !TargetASC)
    {
        return false;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    AActor* EffectCauser = BulletInfo.Actor ? Cast<AActor>(BulletInfo.Actor.Get()) : SourceActor;
    Context.AddInstigator(SourceActor, EffectCauser);
    Context.AddHitResult(TargetHit, true);

    if (BulletInfo.Actor)
    {
        Context.AddSourceObject(BulletInfo.Actor.Get());
    }
    else if (BulletInfo.Entity)
    {
        Context.AddSourceObject(BulletInfo.Entity.Get());
    }

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(ApplyData->GameplayEffect, ApplyData->EffectLevel, Context);
    if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
    {
        return false;
    }

    for (const TPair<FName, float>& Pair : BulletInfo.InitParams.Payload.SetByCallerNameMagnitudes)
    {
        if (!Pair.Key.IsNone())
        {
            UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(SpecHandle, Pair.Key, Pair.Value);
        }
    }

    for (const TPair<FGameplayTag, float>& Pair : BulletInfo.InitParams.Payload.SetByCallerTagMagnitudes)
    {
        if (Pair.Key.IsValid())
        {
            UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, Pair.Key, Pair.Value);
        }
    }

    if (!ApplyData->DynamicGrantedTags.IsEmpty())
    {
        SpecHandle.Data->DynamicGrantedTags.AppendTags(ApplyData->DynamicGrantedTags);
    }

    const FActiveGameplayEffectHandle ActiveHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
    return ActiveHandle.IsValid();
}

FHitResult UBulletLogicController_ApplyGameplayEffect::BuildBestEffortHitForTarget(const FBulletInfo& BulletInfo, const FHitResult& FallbackHit, AActor* TargetActor)
{
    if (!TargetActor)
    {
        return FallbackHit;
    }

    if (FallbackHit.GetActor() == TargetActor)
    {
        return FallbackHit;
    }

    const FVector HitLocation = TargetActor->GetActorLocation();
    const FVector TraceStart = BulletInfo.RayInfo.TraceStart.IsNearlyZero() ? FallbackHit.TraceStart : BulletInfo.RayInfo.TraceStart;
    const FVector TraceEnd = BulletInfo.RayInfo.TraceEnd.IsNearlyZero() ? FallbackHit.TraceEnd : BulletInfo.RayInfo.TraceEnd;
    const FVector HitNormal = (TraceEnd - TraceStart).GetSafeNormal();

    FHitResult OutHit(TargetActor, nullptr, HitLocation, HitNormal);
    OutHit.Location = HitLocation;
    OutHit.ImpactPoint = HitLocation;
    OutHit.TraceStart = TraceStart;
    OutHit.TraceEnd = TraceEnd;
    OutHit.ImpactNormal = HitNormal;
    return OutHit;
}

void UBulletLogicController_ApplyGameplayEffect::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    UBulletLogicData_ApplyGameplayEffect* ApplyData = Cast<UBulletLogicData_ApplyGameplayEffect>(Data);
    if (!ApplyData || !Controller)
    {
        return;
    }

    if (ShouldSkipByNetMode(ApplyData))
    {
        return;
    }

    if (!ShouldApplyEffect(BulletInfo, Hit))
    {
        return;
    }

    AActor* SourceActor = BulletInfo.InitParams.Owner;

    if (ApplyData->bApplyToAllHitActors)
    {
        const float BatchHitTime = BulletInfo.CollisionInfo.LastHitTime;
        if (FMath::IsNearlyEqual(LastAppliedBatchHitTime, BatchHitTime, 0.001f))
        {
            return;
        }
        LastAppliedBatchHitTime = BatchHitTime;

        for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : BulletInfo.CollisionInfo.HitActors)
        {
            if (!FMath::IsNearlyEqual(Pair.Value, BatchHitTime, 0.001f))
            {
                continue;
            }

            AActor* TargetActor = Pair.Key.Get();
            if (!TargetActor)
            {
                continue;
            }

            const FHitResult TargetHit = BuildBestEffortHitForTarget(BulletInfo, Hit, TargetActor);
            const bool bApplied = ApplyEffectToTarget(ApplyData, BulletInfo, SourceActor, TargetActor, TargetHit);
            OnEffectApplied(BulletInfo, TargetHit, bApplied);
        }
        return;
    }

    AActor* TargetActor = Hit.GetActor();
    if (!TargetActor)
    {
        TargetActor = BulletInfo.InitParams.TargetActor;
    }

    const bool bApplied = ApplyEffectToTarget(ApplyData, BulletInfo, SourceActor, TargetActor, Hit);
    OnEffectApplied(BulletInfo, Hit, bApplied);
}

bool UBulletLogicController_ApplyGameplayEffect::ShouldApplyEffect_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit) const
{
    (void)BulletInfo;
    (void)Hit;
    return true;
}

bool UBulletLogicController_ApplyGameplayEffect::K2_ApplyEffect_Implementation(UBulletLogicData_ApplyGameplayEffect* ApplyData, AActor* SourceActor, AActor* TargetActor, const FHitResult& Hit, float EffectLevel) const
{
    (void)ApplyData;
    (void)SourceActor;
    (void)TargetActor;
    (void)Hit;
    (void)EffectLevel;
    return false;
}

void UBulletLogicController_ApplyGameplayEffect::OnEffectApplied_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit, bool bSuccess)
{
    (void)BulletInfo;
    (void)Hit;
    (void)bSuccess;
}
