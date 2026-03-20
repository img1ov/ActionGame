// BulletSystem: BulletActions.cpp
// Queued lifecycle action pipeline.
#include "Action/BulletActions.h"
#include "Controller/BulletController.h"
#include "Model/BulletInfo.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Actor/BulletActor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameplayTagAssetInterface.h"
#include "NiagaraComponent.h"
#include "Component/BulletBudgetComponent.h"
#include "GameplayTagsManager.h"
#include "BulletLogChannel.h"

// Initialize bullet state and enqueue follow-up init actions.
void UBulletActionInitBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    // Cache whether this bullet qualifies for the fast path (no extra logic/summon/child/interact).
    BulletInfo.bIsSimple = BulletInfo.Config.CheckSimpleBullet();
    BulletInfo.bIsInit = true;
    const float BudgetInterval = BulletInfo.Config.Base.BudgetTickInterval;
    if (BulletInfo.Entity && BulletInfo.Entity->GetBudgetComponent())
    {
        BulletInfo.Entity->GetBudgetComponent()->Initialize(BudgetInterval, BulletInfo.SpawnWorldTime);
    }
    BulletInfo.MoveInfo.bAttachToOwner = BulletInfo.Config.Move.bAttachToOwner;
    BulletInfo.TimeScale = BulletInfo.Config.TimeScale.TimeDilation > 0.0f ? BulletInfo.Config.TimeScale.TimeDilation : 1.0f;
    BulletInfo.Tags.AppendTags(BulletInfo.Config.Logic.LogicTags);

    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        if (!BulletInfo.bIsSimple)
        {
            BulletInfo.Entity->GetLogicComponent()->Initialize(InController, BulletInfo.Entity, BulletInfo.Config.Execution);
        }
    }

    UE_LOG(LogBullet, Verbose, TEXT("InitBullet: InstanceId=%d Simple=%s"), BulletInfo.InstanceId, BulletInfo.bIsSimple ? TEXT("true") : TEXT("false"));

    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::InitHit});
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::InitMove});
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::InitCollision});
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::InitRender});

    if (BulletInfo.Config.Summon.SummonClass)
    {
        InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::SummonEntity});
    }

    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::UpdateAttackerFrozen});

    if (BulletInfo.Config.Render.NiagaraSystem.ToSoftObjectPath().IsValid() || BulletInfo.Config.Render.StaticMesh.ToSoftObjectPath().IsValid())
    {
        InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::UpdateEffect});
    }

    if (BulletInfo.Config.Interact.bEnableInteract || BulletInfo.Config.Obstacle.bEnableObstacle)
    {
        InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::SceneInteract});
    }
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::UpdateLiveTime});
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::TimeScale});
    InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::AfterInit});

    bool bHasChildren = false;
    for (const FBulletDataChild& Child : BulletInfo.Config.Children)
    {
        if (!Child.ChildBulletID.IsNone() && Child.Count > 0)
        {
            bHasChildren = true;
            break;
        }
    }
    if (bHasChildren)
    {
        InController->EnqueueAction(BulletInfo.InstanceId, {EBulletActionType::Child});
    }
}

void UBulletActionInitHit::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.CollisionInfo.HitCount = 0;
    BulletInfo.CollisionInfo.HitActors.Reset();
    BulletInfo.CollisionInfo.LastHitTime = -BIG_NUMBER;
    BulletInfo.CollisionInfo.bHitThisFrame = false;

#if WITH_EDITOR
    UE_LOG(LogBullet, VeryVerbose, TEXT("InitHit: InstanceId=%d"), BulletInfo.InstanceId);
#endif
}

// Compute initial transform/velocity based on config and init params.
void UBulletActionInitMove::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    FVector SpawnLocation = BulletInfo.InitParams.SpawnTransform.GetLocation();
    FRotator SpawnRotation = BulletInfo.InitParams.SpawnTransform.GetRotation().Rotator();

    if (!BulletInfo.Config.Move.bUseSpawnTransform && BulletInfo.InitParams.Owner)
    {
        SpawnLocation = BulletInfo.InitParams.Owner->GetActorLocation();
        SpawnRotation = BulletInfo.InitParams.Owner->GetActorRotation();
    }

    FVector ConfigOffset = BulletInfo.Config.Move.SpawnLocationOffset;
    if (BulletInfo.Config.Move.bSpawnOffsetInOwnerSpace)
    {
        ConfigOffset = SpawnRotation.RotateVector(ConfigOffset);
    }

    SpawnLocation += ConfigOffset;
    SpawnLocation += BulletInfo.InitParams.SpawnOffset;

    FVector BaseDirection = SpawnRotation.Vector();
    FVector Direction = BaseDirection;
    if (BulletInfo.Config.Aimed.bUseAim)
    {
        const FVector TargetLocation = BulletInfo.InitParams.TargetActor ? BulletInfo.InitParams.TargetActor->GetActorLocation() : BulletInfo.InitParams.TargetLocation;
        if (!TargetLocation.IsNearlyZero())
        {
            FVector AimDirection = (TargetLocation - SpawnLocation).GetSafeNormal();
            if (BulletInfo.Config.Aimed.AimAngleTolerance > 0.0f)
            {
                const float Dot = FMath::Clamp(FVector::DotProduct(AimDirection, BaseDirection), -1.0f, 1.0f);
                const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(Dot));
                if (AngleDeg > BulletInfo.Config.Aimed.AimAngleTolerance)
                {
                    AimDirection = BaseDirection;
                }
            }
            Direction = AimDirection;
        }
    }
    else if (BulletInfo.Config.Move.bUseOwnerForward && BulletInfo.InitParams.Owner)
    {
        Direction = BulletInfo.InitParams.Owner->GetActorForwardVector();
    }

    BulletInfo.MoveInfo.Location = SpawnLocation;
    BulletInfo.MoveInfo.LastLocation = SpawnLocation;
    FRotator FinalRotation = Direction.ToOrientationRotator() + BulletInfo.Config.Move.SpawnRotationOffset;
    Direction = FinalRotation.Vector();
    BulletInfo.MoveInfo.Rotation = FinalRotation;
    BulletInfo.MoveInfo.OrbitCenter = SpawnLocation;
    BulletInfo.MoveInfo.bOrbitCenterInitialized = true;
    BulletInfo.MoveInfo.CustomMoveCurve = nullptr;
    BulletInfo.MoveInfo.CustomMoveDuration = 0.0f;
    BulletInfo.MoveInfo.CustomMoveElapsed = 0.0f;
    BulletInfo.MoveInfo.CustomMoveStartLocation = SpawnLocation;
    BulletInfo.MoveInfo.FixedStartLocation = SpawnLocation;
    BulletInfo.MoveInfo.FixedTargetLocation = FVector::ZeroVector;
    BulletInfo.MoveInfo.FixedDuration = 0.0f;
    BulletInfo.MoveInfo.FixedElapsed = 0.0f;

    const FVector BaseVelocity = Direction * BulletInfo.Config.Move.Speed;
    BulletInfo.MoveInfo.Velocity = BaseVelocity + BulletInfo.Config.Move.InitialVelocity;
    BulletInfo.MoveInfo.Acceleration = FVector(0.0f, 0.0f, BulletInfo.Config.Move.Gravity);
    BulletInfo.MoveInfo.bInitialized = true;

    if (BulletInfo.Config.Move.MoveType == EBulletMoveType::FixedDuration)
    {
        FVector TargetLocation = BulletInfo.InitParams.TargetActor ? BulletInfo.InitParams.TargetActor->GetActorLocation() : BulletInfo.InitParams.TargetLocation;
        if (TargetLocation.IsNearlyZero())
        {
            const float Duration = BulletInfo.Config.Move.FixedDuration > 0.0f ? BulletInfo.Config.Move.FixedDuration : BulletInfo.Config.Base.LifeTime;
            TargetLocation = SpawnLocation + (Direction * BulletInfo.Config.Move.Speed * Duration);
        }

        const float Duration = BulletInfo.Config.Move.FixedDuration > 0.0f ? BulletInfo.Config.Move.FixedDuration : BulletInfo.Config.Base.LifeTime;
        BulletInfo.MoveInfo.FixedStartLocation = SpawnLocation;
        BulletInfo.MoveInfo.FixedTargetLocation = TargetLocation;
        BulletInfo.MoveInfo.FixedDuration = Duration;
        BulletInfo.MoveInfo.FixedElapsed = 0.0f;

        if (Duration > 0.0f)
        {
            BulletInfo.MoveInfo.Velocity = (TargetLocation - SpawnLocation) / Duration;
            if (!BulletInfo.MoveInfo.Velocity.IsNearlyZero())
            {
                BulletInfo.MoveInfo.Rotation = BulletInfo.MoveInfo.Velocity.ToOrientationRotator();
            }
        }
    }

    if (BulletInfo.Actor)
    {
        BulletInfo.Actor->SetActorLocationAndRotation(BulletInfo.MoveInfo.Location, BulletInfo.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }

#if WITH_EDITOR
    UE_LOG(LogBullet, VeryVerbose, TEXT("InitMove: Id=%d MoveType=%d Loc=%s Vel=%s"),
        BulletInfo.InstanceId,
        static_cast<int32>(BulletInfo.Config.Move.MoveType),
        *BulletInfo.MoveInfo.Location.ToString(),
        *BulletInfo.MoveInfo.Velocity.ToString());

    if (BulletInfo.Config.Move.MoveType == EBulletMoveType::FixedDuration)
    {
        UE_LOG(LogBullet, VeryVerbose, TEXT("InitMove FixedDuration: Id=%d Duration=%.3f Target=%s"),
            BulletInfo.InstanceId,
            BulletInfo.MoveInfo.FixedDuration,
            *BulletInfo.MoveInfo.FixedTargetLocation.ToString());
    }
#endif
}

// Reset collision caches and enable collision according to config.
void UBulletActionInitCollision::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.CollisionInfo.HitActors.Reset();
    BulletInfo.CollisionInfo.HitCount = 0;
    BulletInfo.CollisionInfo.LastHitTime = -BIG_NUMBER;
    BulletInfo.CollisionInfo.bCollisionEnabled = BulletInfo.Config.Base.bCollisionEnabledOnSpawn;
    BulletInfo.CollisionInfo.OverlapActors.Reset();

#if WITH_EDITOR
    UE_LOG(LogBullet, VeryVerbose, TEXT("InitCollision: Id=%d Enabled=%s Mode=%d Shape=%d HitInterval=%.3f StartDelay=%.3f"),
        BulletInfo.InstanceId,
        BulletInfo.CollisionInfo.bCollisionEnabled ? TEXT("true") : TEXT("false"),
        static_cast<int32>(BulletInfo.Config.Base.CollisionMode),
        static_cast<int32>(BulletInfo.Config.Base.Shape),
        BulletInfo.Config.Base.HitInterval,
        BulletInfo.Config.Base.CollisionStartDelay);
#endif
}

// Spawn/configure render actor and FX if configured.
void UBulletActionInitRender::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    const bool bNeedActor = BulletInfo.Config.Render.ActorClass != nullptr ||
                            BulletInfo.Config.Render.NiagaraSystem.ToSoftObjectPath().IsValid() ||
                            BulletInfo.Config.Render.StaticMesh.ToSoftObjectPath().IsValid();
    if (!bNeedActor)
    {
#if WITH_EDITOR
        UE_LOG(LogBullet, VeryVerbose, TEXT("InitRender skipped: InstanceId=%d (no render config)"), BulletInfo.InstanceId);
#endif
        return;
    }

    ABulletActor* Actor = InController->AcquireBulletActor(BulletInfo);
    BulletInfo.Actor = Actor;
    if (BulletInfo.Entity)
    {
        BulletInfo.Entity->SetActor(Actor);
    }

    if (Actor)
    {
        Actor->SetActorHiddenInGame(false);
        Actor->SetActorEnableCollision(false);
        Actor->SetActorLocationAndRotation(BulletInfo.MoveInfo.Location, BulletInfo.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
        Actor->InitializeRender(BulletInfo.Config.Render);
        BulletInfo.EffectInfo.NiagaraComponent = Actor->GetNiagaraComponent();

        FVector FinalScale = BulletInfo.Config.Scale.Scale * BulletInfo.Size;
        if (BulletInfo.Config.Scale.bUseOwnerScale && BulletInfo.InitParams.Owner)
        {
            FinalScale *= BulletInfo.InitParams.Owner->GetActorScale3D();
        }
        Actor->SetActorScale3D(FinalScale);

        if (BulletInfo.Config.Render.bAttachToOwner && BulletInfo.InitParams.Owner)
        {
            Actor->AttachToActor(BulletInfo.InitParams.Owner, FAttachmentTransformRules::KeepWorldTransform);
        }
    }

#if WITH_EDITOR
    UE_LOG(LogBullet, VeryVerbose, TEXT("InitRender: Id=%d Actor=%s"),
        BulletInfo.InstanceId,
        Actor ? *Actor->GetName() : TEXT("None"));
#endif
}

void UBulletActionTimeScale::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.TimeScale = BulletInfo.Config.TimeScale.TimeDilation > 0.0f ? BulletInfo.Config.TimeScale.TimeDilation : 1.0f;
}

void UBulletActionAfterInit::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        if (!BulletInfo.bIsSimple)
        {
            BulletInfo.Entity->GetLogicComponent()->HandleOnBegin(BulletInfo);
        }
    }
}

void UBulletActionChild::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    if (BulletInfo.Config.Children.Num() == 0)
    {
        return;
    }

    InController->RequestSummonChildren(BulletInfo, EBulletChildSpawnTrigger::OnCreate);
}

// Spawn child bullets via controller helper.
void UBulletActionSummonBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    const FName ChildBulletID = ActionInfo.ChildBulletID;
    const int32 Count = ActionInfo.SpawnCount;
    const float Spread = ActionInfo.SpreadAngle;
    InController->SpawnChildBulletsFromLogic(
        BulletInfo,
        ChildBulletID,
        Count,
        Spread,
        ActionInfo.InheritOwner,
        ActionInfo.InheritTarget,
        ActionInfo.InheritPayload,
        ActionInfo.SpawnLocationOffset,
        ActionInfo.bSpawnLocationOffsetInSpawnSpace,
        ActionInfo.SpawnRotationOffset);
}

// Spawn a non-bullet actor defined by Summon config.
void UBulletActionSummonEntity::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    InController->SummonEntityFromConfig(BulletInfo);
}

// Finalize bullet destruction and release resources.
void UBulletActionDestroyBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    BulletInfo.DestroyWorldTime = InController->GetWorldTimeSeconds();
    BulletInfo.bNeedDestroy = true;
    BulletInfo.CollisionInfo.bCollisionEnabled = false;
    BulletInfo.CollisionInfo.OverlapActors.Reset();
    BulletInfo.CollisionInfo.HitActors.Reset();
    BulletInfo.PendingDestroyDelay = 0.0f;

    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        if (!BulletInfo.bIsSimple)
        {
            BulletInfo.Entity->GetLogicComponent()->HandleOnDestroy(BulletInfo, ActionInfo.DestroyReason);
        }
    }

    if (ActionInfo.bSpawnChildren)
    {
        InController->RequestSummonChildren(BulletInfo, EBulletChildSpawnTrigger::OnDestroy);
    }

    if (BulletInfo.EffectInfo.NiagaraComponent)
    {
        if (UNiagaraComponent* NiagaraComponent = BulletInfo.EffectInfo.NiagaraComponent.Get())
        {
            NiagaraComponent->Deactivate();
        }
        BulletInfo.EffectInfo.NiagaraComponent = nullptr;
    }

    UE_LOG(LogBullet, Verbose, TEXT("DestroyBullet: InstanceId=%d Reason=%d"), BulletInfo.InstanceId, static_cast<int32>(ActionInfo.DestroyReason));

    InController->MarkBulletForDestroy(BulletInfo.InstanceId);
}

void UBulletActionDelayDestroyBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.PendingDestroyDelay = ActionInfo.Delay;
}

void UBulletActionDelayDestroyBullet::Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds)
{
    if (!InController)
    {
        return;
    }

    if (BulletInfo.PendingDestroyDelay <= 0.0f)
    {
        return;
    }

    BulletInfo.PendingDestroyDelay -= DeltaSeconds * BulletInfo.TimeScale;
    if (BulletInfo.PendingDestroyDelay <= 0.0f)
    {
        UE_LOG(LogBullet, Verbose, TEXT("DelayDestroy elapsed: InstanceId=%d"), BulletInfo.InstanceId);
        InController->RequestDestroyBullet(BulletInfo.InstanceId, EBulletDestroyReason::External, true);
    }
}

void UBulletActionSceneInteract::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)ActionInfo;

    if (!InController)
    {
        return;
    }

    // Tag the bullet so collision/logic can detect scene-interaction intent.
    const FGameplayTag InteractTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Bullet.Interact"), false);
    if (InteractTag.IsValid())
    {
        BulletInfo.Tags.AddTag(InteractTag);
    }

    UE_LOG(LogBullet, Verbose, TEXT("SceneInteract enabled: InstanceId=%d"), BulletInfo.InstanceId);
}

void UBulletActionUpdateEffect::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)ActionInfo;

    if (!InController)
    {
        return;
    }

    if (BulletInfo.Actor)
    {
        if (!BulletInfo.EffectInfo.NiagaraComponent)
        {
            BulletInfo.EffectInfo.NiagaraComponent = BulletInfo.Actor->GetNiagaraComponent();
        }

        // Ensure render scale honors owner scale and runtime size override.
        FVector FinalScale = BulletInfo.Config.Scale.Scale * BulletInfo.Size;
        if (BulletInfo.Config.Scale.bUseOwnerScale && BulletInfo.InitParams.Owner)
        {
            FinalScale *= BulletInfo.InitParams.Owner->GetActorScale3D();
        }
        BulletInfo.Actor->SetActorScale3D(FinalScale);
    }
}

void UBulletActionUpdateEffect::Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)DeltaSeconds;

    if (!InController)
    {
        return;
    }

    if (BulletInfo.Actor)
    {
        FVector FinalScale = BulletInfo.Config.Scale.Scale * BulletInfo.Size;
        if (BulletInfo.Config.Scale.bUseOwnerScale && BulletInfo.InitParams.Owner)
        {
            FinalScale *= BulletInfo.InitParams.Owner->GetActorScale3D();
        }
        BulletInfo.Actor->SetActorScale3D(FinalScale);
    }
}

// Reset live time at spawn.
void UBulletActionUpdateLiveTime::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.LiveTime = 0.0f;
}

// Accumulate lifetime and request destruction on expiry.
void UBulletActionUpdateLiveTime::Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds)
{
    if (!InController)
    {
        return;
    }

    BulletInfo.LiveTime += DeltaSeconds * BulletInfo.TimeScale;

    const float LifeTime = BulletInfo.InitParams.OverrideLifeTime > 0.0f ? BulletInfo.InitParams.OverrideLifeTime : BulletInfo.Config.Base.LifeTime;
    if (LifeTime > 0.0f && BulletInfo.LiveTime >= LifeTime)
    {
        UE_LOG(LogBullet, Verbose, TEXT("LifeTime expired: InstanceId=%d"), BulletInfo.InstanceId);
        InController->RequestDestroyBullet(BulletInfo.InstanceId, EBulletDestroyReason::LifeTimeExpired, true);
    }
}

void UBulletActionUpdateAttackerFrozen::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)ActionInfo;

    if (!InController || !BulletInfo.InitParams.Owner)
    {
        return;
    }

    bool bOwnerFrozen = false;
    if (!BulletInfo.Config.Logic.OwnerFrozenTags.IsEmpty())
    {
        FGameplayTagContainer OwnerTags;
        if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(BulletInfo.InitParams.Owner))
        {
            TagInterface->GetOwnedGameplayTags(OwnerTags);
        }
        bOwnerFrozen = OwnerTags.HasAny(BulletInfo.Config.Logic.OwnerFrozenTags);
    }

    BulletInfo.bFrozen = bOwnerFrozen;
    if (bOwnerFrozen)
    {
        BulletInfo.FrozenUntilTime = 0.0f;
    }

    UE_LOG(LogBullet, Verbose, TEXT("UpdateAttackerFrozen: InstanceId=%d Frozen=%s"), BulletInfo.InstanceId, bOwnerFrozen ? TEXT("true") : TEXT("false"));
}

