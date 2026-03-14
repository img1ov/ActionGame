#include "Action/BulletActions.h"
#include "Controller/BulletController.h"
#include "Model/BulletInfo.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Actor/BulletActor.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

void UBulletActionInitBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    BulletInfo.bIsInit = true;
    BulletInfo.MoveInfo.bAttachToOwner = BulletInfo.Config.Move.bAttachToOwner;
    BulletInfo.TimeScale = BulletInfo.Config.TimeScale.TimeDilation > 0.0f ? BulletInfo.Config.TimeScale.TimeDilation : 1.0f;
    BulletInfo.Tags.AppendTags(BulletInfo.Config.Logic.LogicTags);

    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        BulletInfo.Entity->GetLogicComponent()->Initialize(InController, BulletInfo.Entity, BulletInfo.Config.Execution);
    }

    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::InitHit});
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::InitMove});
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::InitCollision});
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::InitRender});

    if (BulletInfo.Config.Summon.SummonClass)
    {
        InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::SummonEntity});
    }

    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::UpdateAttackerFrozen});

    if (BulletInfo.Config.Render.NiagaraSystem.ToSoftObjectPath().IsValid() || BulletInfo.Config.Render.StaticMesh.ToSoftObjectPath().IsValid())
    {
        InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::UpdateEffect});
    }

    if (BulletInfo.Config.Interact.bEnableInteract || BulletInfo.Config.Obstacle.bEnableObstacle)
    {
        InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::SceneInteract});
    }
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::UpdateLiveTime});
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::TimeScale});
    InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::AfterInit});

    if (BulletInfo.Config.Children.Count > 0)
    {
        InController->EnqueueAction(BulletInfo.BulletId, {EBulletActionType::Child});
    }
}

void UBulletActionInitHit::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.CollisionInfo.HitCount = 0;
    BulletInfo.CollisionInfo.HitActors.Reset();
    BulletInfo.CollisionInfo.LastHitTime = 0.0f;
    BulletInfo.CollisionInfo.bHitThisFrame = false;
}

void UBulletActionInitMove::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    FVector SpawnLocation = BulletInfo.InitParams.SpawnTransform.GetLocation();
    FRotator SpawnRotation = BulletInfo.InitParams.SpawnTransform.GetRotation().Rotator();

    if (!BulletInfo.Config.Move.bUseSpawnTransform && BulletInfo.InitParams.Owner)
    {
        SpawnLocation = BulletInfo.InitParams.Owner->GetActorLocation();
        SpawnRotation = BulletInfo.InitParams.Owner->GetActorRotation();
    }

    FVector ConfigOffset = BulletInfo.Config.Move.SpawnOffset;
    if (BulletInfo.Config.Move.bSpawnOffsetInOwnerSpace)
    {
        ConfigOffset = SpawnRotation.RotateVector(ConfigOffset);
    }

    SpawnLocation += ConfigOffset;
    SpawnLocation += BulletInfo.InitParams.SpawnOffset;

    FVector Direction = SpawnRotation.Vector();
    if (BulletInfo.Config.Aimed.bUseAim)
    {
        const FVector TargetLocation = BulletInfo.InitParams.TargetActor ? BulletInfo.InitParams.TargetActor->GetActorLocation() : BulletInfo.InitParams.TargetLocation;
        if (!TargetLocation.IsNearlyZero())
        {
            Direction = (TargetLocation - SpawnLocation).GetSafeNormal();
        }
    }
    else if (BulletInfo.Config.Move.bUseOwnerForward && BulletInfo.InitParams.Owner)
    {
        Direction = BulletInfo.InitParams.Owner->GetActorForwardVector();
    }

    BulletInfo.MoveInfo.Location = SpawnLocation;
    BulletInfo.MoveInfo.LastLocation = SpawnLocation;
    BulletInfo.MoveInfo.Rotation = Direction.ToOrientationRotator();
    BulletInfo.MoveInfo.OrbitCenter = SpawnLocation;

    const FVector BaseVelocity = Direction * BulletInfo.Config.Move.Speed;
    BulletInfo.MoveInfo.Velocity = BaseVelocity + BulletInfo.Config.Move.InitialVelocity;
    BulletInfo.MoveInfo.Acceleration = FVector(0.0f, 0.0f, BulletInfo.Config.Move.Gravity);
    BulletInfo.MoveInfo.bInitialized = true;

    if (BulletInfo.Actor)
    {
        BulletInfo.Actor->SetActorLocationAndRotation(BulletInfo.MoveInfo.Location, BulletInfo.MoveInfo.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
}

void UBulletActionInitCollision::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.CollisionInfo.HitActors.Reset();
    BulletInfo.CollisionInfo.HitCount = 0;
    BulletInfo.CollisionInfo.LastHitTime = 0.0f;
    BulletInfo.CollisionInfo.bCollisionEnabled = BulletInfo.Config.Base.bCollisionEnabledOnSpawn;
    BulletInfo.CollisionInfo.OverlapActors.Reset();
}

void UBulletActionInitRender::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
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

        FVector FinalScale = BulletInfo.Config.Scale.Scale;
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
}

void UBulletActionTimeScale::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.TimeScale = BulletInfo.Config.TimeScale.TimeDilation > 0.0f ? BulletInfo.Config.TimeScale.TimeDilation : 1.0f;
}

void UBulletActionAfterInit::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        BulletInfo.Entity->GetLogicComponent()->HandleOnBegin(BulletInfo);
    }
}

void UBulletActionChild::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    if (BulletInfo.Config.Children.Count <= 0)
    {
        return;
    }

    if (!BulletInfo.Config.Children.bSpawnOnDestroy && !BulletInfo.Config.Children.bSpawnOnHit)
    {
        InController->RequestSummonChildren(BulletInfo);
    }
}

void UBulletActionSummonBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    const FName RowName = ActionInfo.ChildRowName.IsNone() ? BulletInfo.Config.Children.ChildRowName : ActionInfo.ChildRowName;
    InController->SpawnChildBulletsFromLogic(BulletInfo, RowName, BulletInfo.Config.Children.Count, BulletInfo.Config.Children.SpreadAngle);
}

void UBulletActionSummonEntity::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    InController->SummonEntityFromConfig(BulletInfo);
}

void UBulletActionDestroyBullet::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    if (!InController)
    {
        return;
    }

    BulletInfo.bNeedDestroy = true;
    if (BulletInfo.Entity && BulletInfo.Entity->GetLogicComponent())
    {
        BulletInfo.Entity->GetLogicComponent()->HandleOnDestroy(BulletInfo, ActionInfo.DestroyReason);
    }

    if (ActionInfo.bSpawnChildren && BulletInfo.Config.Children.bSpawnOnDestroy)
    {
        InController->RequestSummonChildren(BulletInfo);
    }

    InController->MarkBulletForDestroy(BulletInfo.BulletId);
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
        InController->RequestDestroyBullet(BulletInfo.BulletId, EBulletDestroyReason::External, true);
    }
}

void UBulletActionSceneInteract::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)InController;
    (void)BulletInfo;
    (void)ActionInfo;
}

void UBulletActionUpdateEffect::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)InController;
    (void)BulletInfo;
    (void)ActionInfo;
}

void UBulletActionUpdateEffect::Tick(UBulletController* InController, FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)InController;
    (void)BulletInfo;
    (void)DeltaSeconds;
}

void UBulletActionUpdateLiveTime::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    BulletInfo.LiveTime = 0.0f;
}

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
        InController->RequestDestroyBullet(BulletInfo.BulletId, EBulletDestroyReason::LifeTimeExpired, true);
    }
}

void UBulletActionUpdateAttackerFrozen::Execute(UBulletController* InController, FBulletInfo& BulletInfo, const FBulletActionInfo& ActionInfo)
{
    (void)InController;
    (void)BulletInfo;
    (void)ActionInfo;
}
