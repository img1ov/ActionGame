// BulletSystem: BulletController.cpp
// Runtime controller layer and orchestration.
#include "Controller/BulletController.h"
#include "BulletLogChannels.h"
#include "Config/BulletConfig.h"
#include "Config/BulletSystemSettings.h"
#include "Model/BulletModel.h"
#include "Action/BulletActionCenter.h"
#include "Action/BulletActionRunner.h"
#include "System/BulletMoveSystem.h"
#include "System/BulletCollisionSystem.h"
#include "Pool/BulletPools.h"
#include "Actor/BulletActor.h"
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Interact/BulletInteractInterface.h"
#include "NiagaraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/OverlapResult.h"
#if WITH_EDITOR
#include "Components/LineBatchComponent.h"
#endif

// Initialize runtime subsystems and pools for this world.
void UBulletController::Initialize(UWorld* InWorld)
{
    World = InWorld;

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    bEnableDebugDraw = Settings ? Settings->bEnableDebugDraw : false;

    // Model owns bullet state; pools provide reusable storage to reduce alloc churn.
    Model = NewObject<UBulletModel>(this);
    if (Model && Settings && Settings->InitialBulletCapacity > 0)
    {
        Model->Reserve(Settings->InitialBulletCapacity);
    }
    BulletPool = NewObject<UBulletPool>(this);
    BulletActorPool = NewObject<UBulletActorPool>(this);
    TraceElementPool = NewObject<UBulletTraceElementPool>(this);

    // Action center/runner decouple lifecycle actions from per-frame systems.
    ActionCenter = NewObject<UBulletActionCenter>(this);
    ActionCenter->Initialize(this);

    ActionRunner = NewObject<UBulletActionRunner>(this);
    ActionRunner->Initialize(this, ActionCenter);

    // High-frequency systems run every tick before actions.
    MoveSystem = NewObject<UBulletMoveSystem>(this);
    CollisionSystem = NewObject<UBulletCollisionSystem>(this);

    if (BulletActorPool)
    {
        TSubclassOf<ABulletActor> DefaultActorClass = nullptr;
        if (Settings && Settings->DefaultBulletActorClass.ToSoftObjectPath().IsValid())
        {
            if (Settings->DefaultBulletActorClass.IsValid())
            {
                DefaultActorClass = Settings->DefaultBulletActorClass.Get();
            }
            else
            {
                FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
                DefaultActorClass = Cast<UClass>(Streamable.LoadSynchronous(Settings->DefaultBulletActorClass.ToSoftObjectPath()));
            }
        }
        BulletActorPool->Initialize(InWorld, DefaultActorClass);
    }

    if (MoveSystem)
    {
        MoveSystem->Initialize(this);
    }

    if (CollisionSystem)
    {
        CollisionSystem->Initialize(this);
    }

    UE_LOG(LogBullet, Verbose, TEXT("BulletController initialized. World=%s"), InWorld ? *InWorld->GetName() : TEXT("None"));
}

void UBulletController::Shutdown() const
{
    if (!Model)
    {
        return;
    }

#if WITH_EDITOR
    auto MakeBulletDebugBatchId = [](int32 InInstanceId) -> uint32
    {
        return 0xB011E700u ^ static_cast<uint32>(InInstanceId);
    };
#endif

    // Release all live bullets first (actors/entities/actions), then clear pools.
    TArray<int32> InstanceIds;
    InstanceIds.Reserve(Model->GetBulletMap().Num());
    for (const auto& Pair : Model->GetBulletMap())
    {
        InstanceIds.Add(Pair.Key);
    }

    for (int32 InstanceId : InstanceIds)
    {
        if (ActionRunner)
        {
            ActionRunner->ClearBulletActions(InstanceId);
        }

        FBulletInfo* Info = Model->GetBullet(InstanceId);
        if (!Info)
        {
            continue;
        }

        if (Info->Actor)
        {
            ReleaseBulletActor(Info->Actor);
            Info->Actor = nullptr;
        }

#if WITH_EDITOR
        if (bEnableDebugDraw)
        {
            if (UWorld* WorldPtr = GetWorld())
            {
                ULineBatchComponent* LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::World);
                if (!LineBatcher)
                {
                    LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
                }
                if (LineBatcher)
                {
                    LineBatcher->ClearBatch(MakeBulletDebugBatchId(InstanceId));
                }
            }
        }
#endif

        if (Info->Entity && BulletPool)
        {
            BulletPool->ReleaseEntity(Info->Entity);
            Info->Entity = nullptr;
        }

        Model->RemoveBullet(InstanceId);
    }

    if (BulletActorPool)
    {
        BulletActorPool->Clear();
    }

    if (BulletPool)
    {
        BulletPool->Clear();
    }

    if (TraceElementPool)
    {
        TraceElementPool->Clear();
    }

    Model->Clear();
}

// Main tick: run simulation systems, then process queued actions.
void UBulletController::OnTick(float DeltaSeconds) const
{
    if (!Model)
    {
        return;
    }

    UE_LOG(LogBullet, VeryVerbose, TEXT("BulletController OnTick: Delta=%.4f"), DeltaSeconds);

    if (ActionRunner)
    {
        ActionRunner->Pause();
    }

    // System tick first, then action runner, to keep high-frequency logic separated from lifecycle actions.
    if (MoveSystem)
    {
        MoveSystem->OnTick(DeltaSeconds);
    }

    if (CollisionSystem)
    {
        CollisionSystem->OnTick(DeltaSeconds);
    }

    if (ActionRunner)
    {
        ActionRunner->Resume();
        ActionRunner->Run(DeltaSeconds);
    }
}

void UBulletController::OnAfterTick(float DeltaSeconds) const
{
    UE_LOG(LogBullet, VeryVerbose, TEXT("BulletController OnAfterTick: Delta=%.4f"), DeltaSeconds);

    if (ActionRunner)
    {
        ActionRunner->Pause();
    }

    if (MoveSystem)
    {
        MoveSystem->OnAfterTick(DeltaSeconds);
    }

    if (CollisionSystem)
    {
        CollisionSystem->OnAfterTick(DeltaSeconds);
    }

    if (ActionRunner)
    {
        ActionRunner->Resume();
        ActionRunner->AfterTick(DeltaSeconds);
    }

    FlushDestroyedBullets();
}

// Resolve config by BulletId and enqueue initial actions.
bool UBulletController::SpawnBullet(const FBulletInitParams& InitParams, FName BulletId, int32& OutInstanceId, const UBulletConfig* OverrideConfig) const
{
    if (BulletId.IsNone())
    {
        return false;
    }

    FBulletDataMain Data;
    bool bFound = false;

    if (OverrideConfig)
    {
        bFound = OverrideConfig->GetBulletData(BulletId, Data);
    }

    if (!bFound)
    {
        UE_LOG(LogBullet, Warning, TEXT("BulletController: Bullet '%s' not found (Config=%s)."),
            *BulletId.ToString(),
            *GetNameSafe(OverrideConfig));
        return false;
    }

    UE_LOG(LogBullet, Verbose, TEXT("SpawnBullet: BulletId=%s Owner=%s Config=%s"),
        *BulletId.ToString(),
        InitParams.Owner ? *InitParams.Owner->GetName() : TEXT("None"),
        *GetNameSafe(OverrideConfig));

    UBulletConfig* SourceConfigAsset = nullptr;
    if (OverrideConfig)
    {
        SourceConfigAsset = const_cast<UBulletConfig*>(OverrideConfig);
    }

    return SpawnBulletByDataInternal(InitParams, Data, OutInstanceId, SourceConfigAsset);
}

// Allocate a bullet entry and enqueue its init action.
bool UBulletController::SpawnBulletByData(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutInstanceId) const
{
    return SpawnBulletByDataInternal(InitParams, Data, OutInstanceId, /*SourceConfigAsset*/ nullptr);
}

bool UBulletController::SpawnBulletByDataInternal(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutInstanceId, UBulletConfig* SourceConfigAsset) const
{
    if (!Model)
    {
        return false;
    }

    // Allocate from pool to avoid frequent memory churn.
    FBulletInfo* Info = Model->SpawnBullet(BulletPool, InitParams, Data);
    if (!Info)
    {
        return false;
    }

    Info->SourceConfigAsset = SourceConfigAsset;
    Info->SpawnWorldTime = GetWorldTimeSeconds();
    OutInstanceId = Info->InstanceId;
    // Kick off the init action chain.
    EnqueueAction(OutInstanceId, {EBulletActionType::InitBullet});

    // Make the instance usable immediately for gameplay code that calls into the bullet API right after spawning.
    // (e.g. manual-hit processing in the same frame). If actions are already running, we keep it deferred.
    if (ActionRunner && !ActionRunner->IsRunning())
    {
        ActionRunner->RunQueuedActionsForBullet(OutInstanceId);
    }

    UE_LOG(LogBullet, Verbose, TEXT("Bullet created: InstanceId=%d BulletId=%s Simple=%s SourceConfig=%s"),
        OutInstanceId,
        *Data.BulletId.ToString(),
        Info->bIsSimple ? TEXT("true") : TEXT("false"),
        *GetNameSafe(SourceConfigAsset));
    return true;
}

void UBulletController::EnqueueAction(int32 InstanceId, const FBulletActionInfo& ActionInfo) const
{
    if (!ActionRunner)
    {
        return;
    }

    if (ActionCenter)
    {
        FBulletActionInfo PooledInfo = ActionCenter->AcquireActionInfo();
        PooledInfo = ActionInfo;
        ActionRunner->EnqueueAction(InstanceId, PooledInfo);
        return;
    }

    ActionRunner->EnqueueAction(InstanceId, ActionInfo);
}

void UBulletController::RequestDestroyBullet(int32 InstanceId) const
{
    RequestDestroyBulletInternal(InstanceId, /*bSummonChildrenOnDestroy*/ true);
}

void UBulletController::RequestDestroyBulletInternal(int32 InstanceId, bool bSummonChildrenOnDestroy) const
{
    if (!Model)
    {
        return;
    }

    FBulletInfo* BulletInfo = Model->GetBullet(InstanceId);
    if (!BulletInfo || BulletInfo->bNeedDestroy)
    {
        return;
    }

#if WITH_EDITOR
    if (bEnableDebugDraw)
    {
        if (UWorld* WorldPtr = GetWorld())
        {
            ULineBatchComponent* LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::World);
            if (!LineBatcher)
            {
                LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
            }
            if (LineBatcher)
            {
                const uint32 BatchId = 0xB011E700u ^ static_cast<uint32>(InstanceId);
                LineBatcher->ClearBatch(BatchId);
            }
        }
    }
#endif

    BulletInfo->DestroyWorldTime = GetWorldTimeSeconds();
    BulletInfo->bNeedDestroy = true;
    BulletInfo->CollisionInfo.bCollisionEnabled = false;
    BulletInfo->CollisionInfo.OverlapActors.Reset();
    BulletInfo->CollisionInfo.HitActors.Reset();
    BulletInfo->PendingDestroyDelay = 0.0f;

    if (BulletInfo->Entity && BulletInfo->Entity->GetLogicComponent())
    {
        if (!BulletInfo->bIsSimple)
        {
            BulletInfo->Entity->GetLogicComponent()->HandleOnDestroy(*BulletInfo);
        }
    }

    // Child spawning is governed by bullet config (bSpawnOnDestroy), not by the destroy request.
    if (bSummonChildrenOnDestroy)
    {
        RequestSummonChildren(*BulletInfo, EBulletChildSpawnTrigger::OnDestroy);
    }

    if (BulletInfo->EffectInfo.NiagaraComponent)
    {
        if (UNiagaraComponent* NiagaraComponent = BulletInfo->EffectInfo.NiagaraComponent.Get())
        {
            NiagaraComponent->Deactivate();
        }
        BulletInfo->EffectInfo.NiagaraComponent = nullptr;
    }

    MarkBulletForDestroy(InstanceId);

    UE_LOG(LogBullet, Verbose, TEXT("RequestDestroyBullet: InstanceId=%d SummonChildren=%s"),
        InstanceId,
        bSummonChildrenOnDestroy ? TEXT("true") : TEXT("false"));
}

void UBulletController::MarkBulletForDestroy(int32 InstanceId) const
{
    if (Model)
    {
        Model->MarkNeedDestroy(InstanceId);
    }
}

void UBulletController::CollectManualHitCandidates(FBulletInfo& Info, TArray<FHitResult>& OutHits) const
{
    OutHits.Reset();
    Info.CollisionInfo.OverlapActors.Reset();

    UWorld* WorldPtr = GetWorld();
    if (!WorldPtr)
    {
        return;
    }

#if WITH_EDITOR
    // Manual hitboxes don't run inside CollisionSystem anymore, so draw debug only when ProcessManualHits queries.
    ULineBatchComponent* DebugLineBatcher = nullptr;
    constexpr float DebugDrawLifeTime = 2.0f;
    const uint32 DebugBatchId = 0xB011E700u ^ static_cast<uint32>(Info.InstanceId);
    if (bEnableDebugDraw)
    {
        DebugLineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::World);
        if (!DebugLineBatcher)
        {
            DebugLineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
        }
        if (DebugLineBatcher)
        {
            DebugLineBatcher->ClearBatch(DebugBatchId);
        }
    }
#endif

    // Manual-hit processing can be invoked from anim notifies in between bullet move ticks.
    // If the bullet is configured to follow/attach to its owner, ensure we sync the latest transform here so the
    // overlap/sweep query matches what the player sees (and isn't affected by budgeting move-tick decimation).
    const bool bShouldAttachToOwner = (Info.MoveInfo.bAttachToOwner || Info.Config.Move.MoveType == EBulletMoveType::Attached);
    if (bShouldAttachToOwner && Info.InitParams.Owner)
    {
        const FTransform OwnerTransform = Info.InitParams.Owner->GetActorTransform();
        if (!Info.MoveInfo.bAttachedLastTick)
        {
            const FTransform BulletTransform(Info.MoveInfo.Rotation, Info.MoveInfo.Location);
            Info.MoveInfo.AttachedRelativeTransform = BulletTransform.GetRelativeTransform(OwnerTransform);
        }

        Info.MoveInfo.LastLocation = Info.MoveInfo.Location;
        const FTransform NewBulletTransform = Info.MoveInfo.AttachedRelativeTransform * OwnerTransform;
        Info.MoveInfo.Location = NewBulletTransform.GetLocation();
        Info.MoveInfo.Rotation = NewBulletTransform.GetRotation().Rotator();
        Info.MoveInfo.bAttachedLastTick = true;
    }

    if (Info.bNeedDestroy)
    {
        return;
    }

    if (!Info.CollisionInfo.bCollisionEnabled)
    {
        return;
    }

    const float WorldTime = GetWorldTimeSeconds();
    if (Info.Config.Base.CollisionStartDelay > 0.0f && (WorldTime - Info.SpawnWorldTime) < Info.Config.Base.CollisionStartDelay)
    {
        return;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BulletManualHit), false);
    if (Info.Actor)
    {
        QueryParams.AddIgnoredActor(Info.Actor);
    }
    if (Info.InitParams.Owner)
    {
        QueryParams.AddIgnoredActor(Info.InitParams.Owner);
    }

    const ECollisionChannel Channel = Info.Config.Base.CollisionChannel;
    const bool bOverlapMode = Info.Config.Base.CollisionMode == EBulletCollisionMode::Overlap;

    // Unique hits per actor (avoid multi-component duplicates in overlap/sweep).
    TMap<TWeakObjectPtr<AActor>, FHitResult> HitsByActor;
    auto AddHitForActor = [&HitsByActor](AActor* HitActor, const FHitResult& Hit)
    {
        if (!HitActor)
        {
            return;
        }

        // Keep the first hit we see for an actor to keep behavior stable and cheap.
        if (!HitsByActor.Contains(HitActor))
        {
            HitsByActor.Add(HitActor, Hit);
        }
    };

    const FVector Center = Info.MoveInfo.Location;
    const FQuat Rotation = Info.MoveInfo.Rotation.Quaternion();

    if (bOverlapMode)
    {
        TSet<TWeakObjectPtr<AActor>> NewOverlaps;

        switch (Info.Config.Base.Shape)
        {
        case EBulletShapeType::Sphere:
        {
            const float Radius = Info.Config.Base.SphereRadius;
            TArray<FOverlapResult> Overlaps;
            WorldPtr->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
            for (const FOverlapResult& Overlap : Overlaps)
            {
                if (AActor* HitActor = Overlap.GetActor())
                {
                    NewOverlaps.Add(HitActor);
                }
            }
            break;
        }
        case EBulletShapeType::Box:
        {
            const FVector Extent = Info.Config.Base.BoxExtent;
            TArray<FOverlapResult> Overlaps;
            WorldPtr->OverlapMultiByChannel(Overlaps, Center, Rotation, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
            for (const FOverlapResult& Overlap : Overlaps)
            {
                if (AActor* HitActor = Overlap.GetActor())
                {
                    NewOverlaps.Add(HitActor);
                }
            }
            break;
        }
        case EBulletShapeType::Capsule:
        {
            const float Radius = Info.Config.Base.CapsuleRadius;
            const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
            TArray<FOverlapResult> Overlaps;
            WorldPtr->OverlapMultiByChannel(Overlaps, Center, Rotation, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
            for (const FOverlapResult& Overlap : Overlaps)
            {
                if (AActor* HitActor = Overlap.GetActor())
                {
                    NewOverlaps.Add(HitActor);
                }
            }
            break;
        }
        case EBulletShapeType::Ray:
        default:
        {
            const FVector Start = Info.MoveInfo.LastLocation;
            const FVector End = Info.MoveInfo.Location;
            TArray<FHitResult> Hits;
            WorldPtr->LineTraceMultiByChannel(Hits, Start, End, Channel, QueryParams);
            for (const FHitResult& Hit : Hits)
            {
                if (AActor* HitActor = Hit.GetActor())
                {
                    NewOverlaps.Add(HitActor);
                    AddHitForActor(HitActor, Hit);
                }
            }
            break;
        }
        }

        Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);

        // Overlap queries don't provide hit results. Create stable pseudo hit results (same approach as collision system).
        for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
        {
            AActor* HitActor = ActorPtr.Get();
            if (!HitActor)
            {
                continue;
            }

            const FVector HitLocation = HitActor->GetActorLocation();
            const FVector HitNormal = (HitLocation - Center).GetSafeNormal();
            FHitResult Hit(HitActor, nullptr, HitLocation, HitNormal);
            Hit.Location = HitLocation;
            Hit.ImpactPoint = HitLocation;
            Hit.TraceStart = Center;
            Hit.TraceEnd = Center;
            Hit.ImpactNormal = HitNormal;
            AddHitForActor(HitActor, Hit);
        }
    }
    else
    {
        const FVector Start = Info.MoveInfo.LastLocation;
        const FVector End = Info.MoveInfo.Location;

        // If we didn't move this tick, fall back to a stationary overlap test for volume shapes so manual hit works
        // even for "standing" hitboxes configured with Sweep.
        const bool bNoMovement = Start.Equals(End, KINDA_SMALL_NUMBER);
        if (bNoMovement && Info.Config.Base.Shape != EBulletShapeType::Ray)
        {
            TSet<TWeakObjectPtr<AActor>> NewOverlaps;
            switch (Info.Config.Base.Shape)
            {
            case EBulletShapeType::Sphere:
            {
                const float Radius = Info.Config.Base.SphereRadius;
                TArray<FOverlapResult> Overlaps;
                WorldPtr->OverlapMultiByChannel(Overlaps, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                break;
            }
            case EBulletShapeType::Box:
            {
                const FVector Extent = Info.Config.Base.BoxExtent;
                TArray<FOverlapResult> Overlaps;
                WorldPtr->OverlapMultiByChannel(Overlaps, End, Rotation, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                break;
            }
            case EBulletShapeType::Capsule:
            {
                const float Radius = Info.Config.Base.CapsuleRadius;
                const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
                TArray<FOverlapResult> Overlaps;
                WorldPtr->OverlapMultiByChannel(Overlaps, End, Rotation, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
                for (const FOverlapResult& Overlap : Overlaps)
                {
                    if (AActor* HitActor = Overlap.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                    }
                }
                break;
            }
            default:
                break;
            }

            Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);
            for (const TWeakObjectPtr<AActor>& ActorPtr : Info.CollisionInfo.OverlapActors)
            {
                AActor* HitActor = ActorPtr.Get();
                if (!HitActor)
                {
                    continue;
                }

                const FVector HitLocation = HitActor->GetActorLocation();
                const FVector HitNormal = (HitLocation - End).GetSafeNormal();
                FHitResult Hit(HitActor, nullptr, HitLocation, HitNormal);
                Hit.Location = HitLocation;
                Hit.ImpactPoint = HitLocation;
                Hit.TraceStart = End;
                Hit.TraceEnd = End;
                Hit.ImpactNormal = HitNormal;
                AddHitForActor(HitActor, Hit);
            }
        }
        else if (!bNoMovement)
        {
            TArray<FHitResult> Hits;
            bool bHit = false;

            switch (Info.Config.Base.Shape)
            {
            case EBulletShapeType::Sphere:
            {
                const float Radius = Info.Config.Base.SphereRadius;
                bHit = WorldPtr->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), QueryParams);
                break;
            }
            case EBulletShapeType::Box:
            {
                const FVector Extent = Info.Config.Base.BoxExtent;
                bHit = WorldPtr->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeBox(Extent), QueryParams);
                break;
            }
            case EBulletShapeType::Capsule:
            {
                const float Radius = Info.Config.Base.CapsuleRadius;
                const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
                bHit = WorldPtr->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams);
                break;
            }
            case EBulletShapeType::Ray:
            default:
                bHit = WorldPtr->LineTraceMultiByChannel(Hits, Start, End, Channel, QueryParams);
                break;
            }

            if (bHit)
            {
                TSet<TWeakObjectPtr<AActor>> NewOverlaps;
                for (const FHitResult& Hit : Hits)
                {
                    if (AActor* HitActor = Hit.GetActor())
                    {
                        NewOverlaps.Add(HitActor);
                        AddHitForActor(HitActor, Hit);
                    }
                }
                Info.CollisionInfo.OverlapActors = MoveTemp(NewOverlaps);
            }
        }
    }

    HitsByActor.GenerateValueArray(OutHits);

#if WITH_EDITOR
    if (DebugLineBatcher)
    {
        const bool bAnyHit = HitsByActor.Num() > 0;
        const FLinearColor Color = bAnyHit ? FLinearColor::Red : FLinearColor::Green;

        const FVector DebugStart = Info.MoveInfo.LastLocation;
        const FVector DebugEnd = Info.MoveInfo.Location;

        // For sweep mode, draw at End (matches CollisionSystem).
        const FVector DrawCenter = bOverlapMode ? Center : DebugEnd;

        switch (Info.Config.Base.Shape)
        {
        case EBulletShapeType::Sphere:
        {
            const float Radius = Info.Config.Base.SphereRadius;
            DebugLineBatcher->DrawSphere(DrawCenter, Radius, 12, Color, DebugDrawLifeTime, /*DepthPriority*/ 0, /*Thickness*/ 0.0f, DebugBatchId);
            break;
        }
        case EBulletShapeType::Box:
        {
            const FVector Extent = Info.Config.Base.BoxExtent;
            DebugLineBatcher->DrawBox(DrawCenter, Extent, Rotation, Color, DebugDrawLifeTime, /*DepthPriority*/ 0, /*Thickness*/ 0.0f, DebugBatchId);
            break;
        }
        case EBulletShapeType::Capsule:
        {
            const float Radius = Info.Config.Base.CapsuleRadius;
            const float HalfHeight = Info.Config.Base.CapsuleHalfHeight;
            DebugLineBatcher->DrawCapsule(DrawCenter, HalfHeight, Radius, Rotation, Color, DebugDrawLifeTime, /*DepthPriority*/ 0, /*Thickness*/ 0.0f, DebugBatchId);
            break;
        }
        case EBulletShapeType::Ray:
        default:
            DebugLineBatcher->DrawLine(DebugStart, DebugEnd, Color, /*DepthPriority*/ 0, /*Thickness*/ 0.0f, DebugDrawLifeTime, DebugBatchId);
            break;
        }
    }
#endif
}

void UBulletController::GetChildInstanceIds(int32 ParentInstanceId, TArray<int32>& OutChildren) const
{
    OutChildren.Reset();
    if (Model)
    {
        Model->GetChildBullets(ParentInstanceId, OutChildren);
    }
}

int32 UBulletController::GetParentInstanceId(int32 ChildInstanceId) const
{
    return Model ? Model->GetParentInstanceId(ChildInstanceId) : INDEX_NONE;
}

bool UBulletController::SetCollisionEnabled(int32 InstanceId, bool bEnabled, bool bClearOverlaps) const
{
    if (!Model)
    {
        return false;
    }

    FBulletInfo* Info = Model->GetBullet(InstanceId);
    if (!Info)
    {
        return false;
    }

    Info->CollisionInfo.bCollisionEnabled = bEnabled;

    if (!bEnabled && bClearOverlaps)
    {
        Info->CollisionInfo.OverlapActors.Reset();
    }

    return true;
}

int32 UBulletController::ProcessManualHits(int32 InstanceId, bool bApplyCollisionResponse) const
{
    if (!Model)
    {
        return 0;
    }

    FBulletInfo* Info = Model->GetBullet(InstanceId);
    if (!Info || Info->bNeedDestroy)
    {
        return 0;
    }

    if (!Info->CollisionInfo.bCollisionEnabled)
    {
        return 0;
    }

    if (Info->Config.Base.CollisionStartDelay > 0.0f && (GetWorldTimeSeconds() - Info->SpawnWorldTime) < Info->Config.Base.CollisionStartDelay)
    {
        return 0;
    }

    // Manual hits are driven by explicit anim notifies (usually single hit frames).
    // Always reset hit caches up-front so every ProcessManualHits call is a fresh batch.
    Info->CollisionInfo.HitActors.Reset();
    Info->CollisionInfo.HitCount = 0;
    Info->CollisionInfo.LastHitTime = -BIG_NUMBER;
    Info->CollisionInfo.bHitThisFrame = false;
    Info->CollisionInfo.LastBatchHitActors.Reset();

    TArray<FHitResult> Hits;
    CollectManualHitCandidates(*Info, Hits);

    const float WorldTime = GetWorldTimeSeconds();
    struct FPendingManualHit
    {
        AActor* Actor = nullptr;
        FHitResult Hit;
    };

    // Phase 1: filter candidates without dispatching OnHit yet.
    TArray<FPendingManualHit> Pending;
    Pending.Reserve(Hits.Num());
    for (const FHitResult& Hit : Hits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor)
        {
            continue;
        }

        if (!Info->bIsSimple && Info->Entity && Info->Entity->GetLogicComponent())
        {
            if (!Info->Entity->GetLogicComponent()->FilterHit(*Info, Hit))
            {
                continue;
            }
        }

        Pending.Add(FPendingManualHit{ HitActor, Hit });
    }

    if (Pending.Num() == 0)
    {
        return 0;
    }

    // Phase 2: determine which accepted hits should be processed this call (respects destroy-on-hit / max-hit limits).
    // We want HitActors to include the *full processed batch* before the first OnHit controller runs.
    TArray<FPendingManualHit> ToProcess;
    ToProcess.Reserve(Pending.Num());
    int32 SimHitCount = Info->CollisionInfo.HitCount;
    for (const FPendingManualHit& Entry : Pending)
    {
        SimHitCount++;
        ToProcess.Add(Entry);

        if (!bApplyCollisionResponse)
        {
            continue;
        }

        const bool bHitLimitReached = SimHitCount >= Info->Config.Base.MaxHitCount;
        const bool bDestroyOnHit = Info->Config.Base.bDestroyOnHit || bHitLimitReached;
        const EBulletCollisionResponse Response = Info->Config.Base.CollisionResponse;

        // If this hit would destroy the bullet, no further hits should be processed this call.
        if (bHitLimitReached || ((Response == EBulletCollisionResponse::Destroy || Response == EBulletCollisionResponse::Support) && bDestroyOnHit))
        {
            break;
        }
    }

    // Mark batch hit time and pre-fill HitActors so batch-based logic (e.g. ApplyToAllHitActors) sees a complete set.
    Info->CollisionInfo.LastHitTime = WorldTime;
    Info->CollisionInfo.bHitThisFrame = true;
    Info->CollisionInfo.LastHitBatchId++;
    Info->CollisionInfo.LastBatchHitActors.Reset(ToProcess.Num());
    for (const FPendingManualHit& Entry : ToProcess)
    {
        Info->CollisionInfo.LastBatchHitActors.Add(Entry.Actor);
    }

    int32 AppliedCount = 0;
    for (const FPendingManualHit& Entry : ToProcess)
    {
        Info->CollisionInfo.HitActors.Add(Entry.Actor, WorldTime);
        // Only count consumed hits (destroy-on-hit will stop the loop).
        Info->CollisionInfo.HitCount++;
        const bool bDestroyed = HandleHitResult(*Info, Entry.Actor, Entry.Hit, bApplyCollisionResponse);
        AppliedCount++;
        if (bDestroyed)
        {
            break;
        }
    }

    return AppliedCount;
}

void UBulletController::RequestSummonChildren(const FBulletInfo& ParentInfo, EBulletChildSpawnTrigger Trigger) const
{
    if (ParentInfo.Config.Children.Num() == 0)
    {
        return;
    }

    for (const FBulletDataChild& ChildData : ParentInfo.Config.Children)
    {
        if (ChildData.ChildBulletId.IsNone() || ChildData.Count <= 0)
        {
            continue;
        }

        const bool bSpawnOnDestroy = ChildData.bSpawnOnDestroy;
        const bool bSpawnOnHit = ChildData.bSpawnOnHit;
        const bool bSpawnOnCreate = !bSpawnOnDestroy && !bSpawnOnHit;

        bool bShouldSpawn = false;
        switch (Trigger)
        {
        case EBulletChildSpawnTrigger::OnCreate:
            bShouldSpawn = bSpawnOnCreate;
            break;
        case EBulletChildSpawnTrigger::OnHit:
            bShouldSpawn = bSpawnOnHit;
            break;
        case EBulletChildSpawnTrigger::OnDestroy:
            bShouldSpawn = bSpawnOnDestroy;
            break;
        default:
            break;
        }

        if (!bShouldSpawn)
        {
            continue;
        }

        // Spawn via action to preserve lifecycle ordering and avoid mutating maps mid-tick.
        FBulletActionInfo ActionInfo;
        ActionInfo.Type = EBulletActionType::SummonBullet;
        ActionInfo.ChildBulletId = ChildData.ChildBulletId;
        ActionInfo.SpawnCount = ChildData.Count;
        ActionInfo.SpreadAngle = 0.0f;
        ActionInfo.SpawnLocationOffset = ChildData.SpawnLocationOffset;
        ActionInfo.bSpawnLocationOffsetInSpawnSpace = ChildData.bSpawnLocationOffsetInSpawnSpace;
        ActionInfo.SpawnRotationOffset = ChildData.SpawnRotationOffset;
        ActionInfo.InheritOwner = ChildData.bInheritOwner ? 1 : 0;
        ActionInfo.InheritTarget = ChildData.bInheritTarget ? 1 : 0;
        ActionInfo.InheritPayload = ChildData.bInheritPayload ? 1 : 0;
        EnqueueAction(ParentInfo.InstanceId, ActionInfo);
    }
}

void UBulletController::SpawnChildBulletsFromLogic(
    const FBulletInfo& ParentInfo,
    FName ChildBulletId,
    int32 Count,
    float SpreadAngle,
    int32 InheritOwnerOverride,
    int32 InheritTargetOverride,
    int32 InheritPayloadOverride,
    const FVector& SpawnLocationOffset,
    bool bSpawnLocationOffsetInSpawnSpace,
    const FRotator& SpawnRotationOffset) const
{
    if (ChildBulletId.IsNone())
    {
        return;
    }

    const FVector Origin = ParentInfo.MoveInfo.Location;
    const FRotator BaseRot = ParentInfo.MoveInfo.Rotation;
    const FBulletDataChild* MatchingChild = FindChildEntry(ParentInfo, ChildBulletId);
    const int32 FinalCount = (Count >= 0) ? Count : (MatchingChild ? MatchingChild->Count : 0);
    const float FinalSpread = (SpreadAngle >= 0.0f) ? SpreadAngle : 0.0f;
    if (FinalCount <= 0)
    {
        return;
    }
    const bool bInheritOwner = (InheritOwnerOverride >= 0) ? (InheritOwnerOverride != 0) : (MatchingChild ? MatchingChild->bInheritOwner : true);
    const bool bInheritTarget = (InheritTargetOverride >= 0) ? (InheritTargetOverride != 0) : (MatchingChild ? MatchingChild->bInheritTarget : true);
    const bool bInheritPayload = (InheritPayloadOverride >= 0) ? (InheritPayloadOverride != 0) : (MatchingChild ? MatchingChild->bInheritPayload : true);
    // Copy parent-derived params up front to avoid referencing ParentInfo after map mutations.
    const FBulletInitParams BaseParams = BuildChildParams(ParentInfo, FTransform::Identity, bInheritOwner, bInheritTarget, bInheritPayload);

    for (int32 Index = 0; Index < FinalCount; ++Index)
    {
        const float AngleOffset = (FinalCount == 1) ? 0.0f : FMath::Lerp(-FinalSpread, FinalSpread, static_cast<float>(Index) / (FinalCount - 1));
        const FRotator ChildRot = (BaseRot + FRotator(0.0f, AngleOffset, 0.0f) + SpawnRotationOffset);

        FVector ChildLocation = Origin;
        if (!SpawnLocationOffset.IsNearlyZero())
        {
            const FVector OffsetWorld = bSpawnLocationOffsetInSpawnSpace ? ChildRot.RotateVector(SpawnLocationOffset) : SpawnLocationOffset;
            ChildLocation += OffsetWorld;
        }

        FBulletInitParams ChildParams = BaseParams;
        ChildParams.SpawnTransform = FTransform(ChildRot, ChildLocation);
        int32 ChildId = INDEX_NONE;
        // If the parent was spawned using an override config asset, children should resolve rows from the same asset
        // to avoid falling back to the global config subsystem (which may not have a config assigned).
        const UBulletConfig* ChildConfigOverride = ParentInfo.SourceConfigAsset ? ParentInfo.SourceConfigAsset.Get() : nullptr;
        SpawnBullet(ChildParams, ChildBulletId, ChildId, ChildConfigOverride);
    }
}

void UBulletController::SummonEntityFromConfig(const FBulletInfo& ParentInfo) const
{
    if (!World.IsValid())
    {
        return;
    }

    const FBulletDataSummon& Summon = ParentInfo.Config.Summon;
    if (!Summon.SummonClass)
    {
        return;
    }

    // If the summon actor is replicated, only spawn it on authority to avoid client-side duplicates
    // (client local spawn + server replicated spawn).
    if (UWorld* WorldPtr = World.Get())
    {
        if (WorldPtr->IsNetMode(NM_Client))
        {
            const AActor* CDO = Cast<AActor>(Summon.SummonClass->GetDefaultObject());
            if (CDO && CDO->GetIsReplicated())
            {
                UE_LOG(LogBullet, Verbose, TEXT("SummonEntityFromConfig: Skip replicated SummonClass on client. InstanceId=%d Class=%s"),
                    ParentInfo.InstanceId,
                    *GetNameSafe(Summon.SummonClass));
                return;
            }
        }
    }

    const FTransform SpawnTransform = Summon.SummonOffset * FTransform(ParentInfo.MoveInfo.Rotation, ParentInfo.MoveInfo.Location);
    AActor* SummonedActor = World->SpawnActor<AActor>(Summon.SummonClass, SpawnTransform);
    if (SummonedActor && Summon.bAttachToOwner && ParentInfo.InitParams.Owner)
    {
        SummonedActor->AttachToActor(ParentInfo.InitParams.Owner, FAttachmentTransformRules::KeepWorldTransform);
    }
}

ABulletActor* UBulletController::AcquireBulletActor(const FBulletInfo& Info) const
{
    if (!BulletActorPool)
    {
        return nullptr;
    }

    return BulletActorPool->AcquireActor(Info.Config.Render.ActorClass);
}

void UBulletController::ReleaseBulletActor(ABulletActor* Actor) const
{
    if (BulletActorPool)
    {
        BulletActorPool->ReleaseActor(Actor);
    }
}

int32 UBulletController::FindInstanceIdByActor(const AActor* Actor) const
{
    if (!Actor || !Model)
    {
        return INDEX_NONE;
    }

    for (const auto& Pair : Model->GetBulletMap())
    {
        if (Pair.Value.Actor == Actor)
        {
            return Pair.Key;
        }
    }

    return INDEX_NONE;
}

UBulletModel* UBulletController::GetModel() const
{
    return Model;
}

UBulletTraceElementPool* UBulletController::GetTraceElementPool() const
{
    return TraceElementPool;
}

UWorld* UBulletController::GetWorld() const
{
    return World.Get();
}

float UBulletController::GetWorldTimeSeconds() const
{
    UWorld* WorldPtr = World.Get();
    return WorldPtr ? WorldPtr->GetTimeSeconds() : 0.0f;
}

bool UBulletController::IsDebugDrawEnabled() const
{
    return bEnableDebugDraw;
}

// Apply logic hooks, interact, child spawns, and collision response for a hit.
bool UBulletController::HandleHitResult(FBulletInfo& Info, AActor* HitActor, const FHitResult& Hit, bool bApplyCollisionResponse) const
{
    UE_LOG(LogBullet, Verbose, TEXT("HandleHitResult: InstanceId=%d HitActor=%s"), Info.InstanceId, HitActor ? *HitActor->GetName() : TEXT("None"));

    if (Info.Entity && Info.Entity->GetLogicComponent())
    {
        if (!Info.bIsSimple)
        {
            Info.Entity->GetLogicComponent()->HandleOnHit(Info, Hit);
        }

        if (HitActor && HitActor->IsA<ABulletActor>())
        {
            const int32 OtherInstanceId = FindInstanceIdByActor(HitActor);
            if (OtherInstanceId != INDEX_NONE)
            {
                if (!Info.bIsSimple)
                {
                    Info.Entity->GetLogicComponent()->HandleOnHitBullet(Info, OtherInstanceId);
                }
            }
        }
    }

    if (Info.Config.Interact.bEnableInteract && Info.Config.Interact.bAffectEnvironment && HitActor)
    {
        // Environment-affecting interaction must be authoritative to avoid client double-run.
        // (Cosmetic-only interactions should use a separate config flag/path.)
        if (UWorld* WorldPtr = GetWorld())
        {
            if (WorldPtr->IsNetMode(NM_Client))
            {
                // Client: skip.
            }
            else if (HitActor->GetClass()->ImplementsInterface(UBulletInteractInterface::StaticClass()))
            {
                IBulletInteractInterface::Execute_OnBulletInteract(HitActor, Info, Hit);
                UE_LOG(LogBullet, Verbose, TEXT("SceneInteract: InstanceId=%d Actor=%s"), Info.InstanceId, *HitActor->GetName());
            }
        }
    }

    RequestSummonChildren(Info, EBulletChildSpawnTrigger::OnHit);

    if (!bApplyCollisionResponse)
    {
        return false;
    }

    const bool bHitLimitReached = Info.CollisionInfo.HitCount >= Info.Config.Base.MaxHitCount;
    const bool bDestroyOnHit = Info.Config.Base.bDestroyOnHit || bHitLimitReached;

    if (Info.Config.Base.Shape == EBulletShapeType::Ray)
    {
        Info.RayInfo.TraceStart = Hit.TraceStart;
        Info.RayInfo.TraceEnd = Hit.TraceEnd;
    }

    switch (Info.Config.Base.CollisionResponse)
    {
    case EBulletCollisionResponse::Destroy:
        if (bDestroyOnHit)
        {
            UE_LOG(LogBullet, Verbose, TEXT("Hit response: Destroy (InstanceId=%d)"), Info.InstanceId);
            RequestDestroyBullet(Info.InstanceId);
            return true;
        }
        break;
    case EBulletCollisionResponse::Bounce:
        if (!Hit.ImpactNormal.IsNearlyZero())
        {
            const FVector Reflected = FMath::GetReflectionVector(Info.MoveInfo.Velocity, Hit.ImpactNormal);
            Info.MoveInfo.Velocity = Reflected;
            if (Info.Entity && Info.Entity->GetLogicComponent())
            {
                if (!Info.bIsSimple)
                {
                    Info.Entity->GetLogicComponent()->HandleOnRebound(Info, Hit);
                }
            }
        }
        break;
    case EBulletCollisionResponse::Support:
        // Support response triggers the OnSupport logic hook and may still respect destroy-on-hit.
        if (Info.Entity && Info.Entity->GetLogicComponent())
        {
            if (!Info.bIsSimple)
            {
                Info.Entity->GetLogicComponent()->HandleOnSupport(Info);
            }
        }
        if (bDestroyOnHit)
        {
            UE_LOG(LogBullet, Verbose, TEXT("Hit response: Support->Destroy (InstanceId=%d)"), Info.InstanceId);
            RequestDestroyBullet(Info.InstanceId);
            return true;
        }
        break;
    case EBulletCollisionResponse::Pierce:
    default:
        break;
    }

    if (bHitLimitReached && !Info.bNeedDestroy)
    {
        UE_LOG(LogBullet, Verbose, TEXT("Hit limit reached: Destroy (InstanceId=%d)"), Info.InstanceId);
        RequestDestroyBullet(Info.InstanceId);
        return true;
    }

    return false;
}

// Final cleanup: release actor/entity, clear actions, and remove from model.
void UBulletController::FlushDestroyedBullets() const
{
    if (!Model)
    {
        return;
    }

#if WITH_EDITOR
    auto MakeBulletDebugBatchId = [](int32 InInstanceId) -> uint32
    {
        return 0xB011E700u ^ static_cast<uint32>(InInstanceId);
    };
#endif

    TArray<int32> ToDestroy = Model->GetNeedDestroyBullets().Array();
    for (int32 InstanceId : ToDestroy)
    {
        FBulletInfo* Info = Model->GetBullet(InstanceId);
        if (!Info)
        {
            continue;
        }

        // Propagate parent destruction to existing child bullets (skip those spawned at destroy time).
        if (Info->DestroyWorldTime >= 0.0f)
        {
            TArray<int32> ChildInstanceIds;
            Model->GetChildBullets(InstanceId, ChildInstanceIds);
            for (int32 ChildInstanceId : ChildInstanceIds)
            {
                FBulletInfo* ChildInfo = Model->GetBullet(ChildInstanceId);
                if (!ChildInfo || ChildInfo->bNeedDestroy)
                {
                    continue;
                }

                if (ChildInfo->SpawnWorldTime >= Info->DestroyWorldTime)
                {
                    continue;
                }

                // Parent-destruction propagation: cleanup children without triggering further child spawning.
                RequestDestroyBulletInternal(ChildInstanceId, /*bSummonChildrenOnDestroy*/ false);
            }
        }

        if (ActionRunner)
        {
            ActionRunner->ClearBulletActions(InstanceId);
        }

#if WITH_EDITOR
        if (bEnableDebugDraw)
        {
            if (UWorld* WorldPtr = GetWorld())
            {
                ULineBatchComponent* LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::World);
                if (!LineBatcher)
                {
                    LineBatcher = WorldPtr->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent);
                }
                if (LineBatcher)
                {
                    LineBatcher->ClearBatch(MakeBulletDebugBatchId(InstanceId));
                }
            }
        }
#endif

        if (Info->Actor)
        {
            ReleaseBulletActor(Info->Actor);
            Info->Actor = nullptr;
        }

        if (Info->Entity && BulletPool)
        {
            BulletPool->ReleaseEntity(Info->Entity);
        }

        Model->RemoveBullet(InstanceId);

        UE_LOG(LogBullet, Verbose, TEXT("Bullet destroyed and recycled: InstanceId=%d"), InstanceId);
    }
}

FBulletInitParams UBulletController::BuildChildParams(const FBulletInfo& ParentInfo, const FTransform& ChildTransform, bool bInheritOwner, bool bInheritTarget, bool bInheritPayload) const
{
    FBulletInitParams Params;
    Params.Owner = bInheritOwner ? ParentInfo.InitParams.Owner : nullptr;
    Params.TargetActor = bInheritTarget ? ParentInfo.InitParams.TargetActor : nullptr;
    Params.TargetLocation = ParentInfo.InitParams.TargetLocation;
    Params.SpawnTransform = ChildTransform;
    if (bInheritPayload)
    {
        Params.Payload = ParentInfo.InitParams.Payload;
    }
    Params.ParentInstanceId = ParentInfo.InstanceId;
    return Params;
}

const FBulletDataChild* UBulletController::FindChildEntry(const FBulletInfo& ParentInfo, FName ChildBulletId) const
{
    if (ChildBulletId.IsNone())
    {
        return nullptr;
    }

    for (const FBulletDataChild& Child : ParentInfo.Config.Children)
    {
        if (Child.ChildBulletId == ChildBulletId)
        {
            return &Child;
        }
    }

    return nullptr;
}
