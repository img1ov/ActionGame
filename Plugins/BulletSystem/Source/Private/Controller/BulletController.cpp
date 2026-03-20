// BulletSystem: BulletController.cpp
// Runtime controller layer and orchestration.
#include "Controller/BulletController.h"
#include "BulletLogChannel.h"
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
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// Initialize runtime subsystems and pools for this world.
void UBulletController::Initialize(UWorld* InWorld)
{
    World = InWorld;

    UGameInstance* GameInstance = InWorld ? InWorld->GetGameInstance() : nullptr;
    ConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UBulletConfigSubsystem>() : nullptr;

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
    if (ActionRunner)
    {
        // Release persistent actions first.
        for (const auto& Pair : Model->GetBulletMap())
        {
            ActionRunner->ClearBulletActions(Pair.Key);
        }
    }

    if (BulletActorPool)
    {
        BulletActorPool->Clear();
    }

    if (Model)
    {
        Model->Clear();
    }
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

    if (ConfigSubsystem)
    {
        ConfigSubsystem->TickPreload(DeltaSeconds);
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

// Resolve config by BulletID and enqueue initial actions.
bool UBulletController::SpawnBullet(const FBulletInitParams& InitParams, FName BulletID, int32& OutBulletId, const UBulletConfig* OverrideConfig) const
{
    if (BulletID.IsNone())
    {
        return false;
    }

    FBulletDataMain Data;
    bool bFound = false;

    if (OverrideConfig)
    {
        bFound = OverrideConfig->GetBulletData(BulletID, Data);
    }
    else if (ConfigSubsystem)
    {
        bFound = ConfigSubsystem->GetBulletData(BulletID, Data, InitParams.Owner);
    }

    if (!bFound)
    {
        UE_LOG(LogBullet, Warning, TEXT("BulletController: Bullet '%s' not found."), *BulletID.ToString());
        return false;
    }

    UE_LOG(LogBullet, Verbose, TEXT("SpawnBullet: BulletID=%s Owner=%s"), *BulletID.ToString(), InitParams.Owner ? *InitParams.Owner->GetName() : TEXT("None"));
    return SpawnBulletByData(InitParams, Data, OutBulletId);
}

// Allocate a bullet entry and enqueue its init action.
bool UBulletController::SpawnBulletByData(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutBulletId) const
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

    Info->SpawnWorldTime = GetWorldTimeSeconds();
    OutBulletId = Info->BulletId;
    // Kick off the init action chain.
    EnqueueAction(OutBulletId, {EBulletActionType::InitBullet});

    if (ConfigSubsystem)
    {
        ConfigSubsystem->RequestPreload(Data);
    }

    UE_LOG(LogBullet, Verbose, TEXT("Bullet created: Id=%d BulletID=%s Simple=%s"), OutBulletId, *Data.BulletID.ToString(), Info->bIsSimple ? TEXT("true") : TEXT("false"));
    return true;
}

void UBulletController::EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo) const
{
    if (!ActionRunner)
    {
        return;
    }

    if (ActionCenter)
    {
        FBulletActionInfo PooledInfo = ActionCenter->AcquireActionInfo();
        PooledInfo = ActionInfo;
        ActionRunner->EnqueueAction(BulletId, PooledInfo);
        return;
    }

    ActionRunner->EnqueueAction(BulletId, ActionInfo);
}

void UBulletController::RequestDestroyBullet(int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren) const
{
    FBulletActionInfo Info;
    Info.Type = EBulletActionType::DestroyBullet;
    Info.DestroyReason = Reason;
    Info.bSpawnChildren = bSpawnChildren;
    EnqueueAction(BulletId, Info);

    if (Model)
    {
        if (FBulletInfo* BulletInfo = Model->GetBullet(BulletId))
        {
            if (BulletInfo->DestroyWorldTime < 0.0f)
            {
                BulletInfo->DestroyWorldTime = GetWorldTimeSeconds();
            }
        }
    }

    UE_LOG(LogBullet, Verbose, TEXT("RequestDestroyBullet: Id=%d Reason=%d SpawnChildren=%s"), BulletId, static_cast<int32>(Reason), bSpawnChildren ? TEXT("true") : TEXT("false"));
}

void UBulletController::MarkBulletForDestroy(int32 BulletId) const
{
    if (Model)
    {
        Model->MarkNeedDestroy(BulletId);
    }
}

void UBulletController::GetChildBulletIds(int32 ParentBulletId, TArray<int32>& OutChildren) const
{
    OutChildren.Reset();
    if (Model)
    {
        Model->GetChildBullets(ParentBulletId, OutChildren);
    }
}

int32 UBulletController::GetParentBulletId(int32 ChildBulletId) const
{
    return Model ? Model->GetParentBulletId(ChildBulletId) : INDEX_NONE;
}

bool UBulletController::SetCollisionEnabled(int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors) const
{
    if (!Model)
    {
        return false;
    }

    FBulletInfo* Info = Model->GetBullet(BulletId);
    if (!Info)
    {
        return false;
    }

    Info->CollisionInfo.bCollisionEnabled = bEnabled;

    if (!bEnabled && bClearOverlaps)
    {
        Info->CollisionInfo.OverlapActors.Reset();
    }

    if (bResetHitActors)
    {
        Info->CollisionInfo.HitActors.Reset();
        Info->CollisionInfo.HitCount = 0;
        Info->CollisionInfo.LastHitTime = -BIG_NUMBER;
    }

    return true;
}

bool UBulletController::ResetHitActors(int32 BulletId) const
{
    if (!Model)
    {
        return false;
    }

    FBulletInfo* Info = Model->GetBullet(BulletId);
    if (!Info)
    {
        return false;
    }

    Info->CollisionInfo.HitActors.Reset();
    Info->CollisionInfo.HitCount = 0;
    Info->CollisionInfo.LastHitTime = -BIG_NUMBER;
    return true;
}

int32 UBulletController::ProcessManualHits(int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse) const
{
    if (!Model)
    {
        return 0;
    }

    FBulletInfo* Info = Model->GetBullet(BulletId);
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

    if (bResetHitActorsBefore)
    {
        Info->CollisionInfo.HitActors.Reset();
        Info->CollisionInfo.HitCount = 0;
        Info->CollisionInfo.LastHitTime = -BIG_NUMBER;
    }

    if (Info->CollisionInfo.OverlapActors.Num() == 0)
    {
        return 0;
    }

    const float WorldTime = GetWorldTimeSeconds();
    const float HitInterval = Info->Config.Base.HitInterval;
    int32 AppliedCount = 0;

    for (const TWeakObjectPtr<AActor>& ActorPtr : Info->CollisionInfo.OverlapActors)
    {
        AActor* HitActor = ActorPtr.Get();
        if (!HitActor)
        {
            continue;
        }

        if (const float* LastHit = Info->CollisionInfo.HitActors.Find(HitActor))
        {
            // HitInterval <= 0 means "no gating" (hit every time).
            if (HitInterval > 0.0f && (WorldTime - *LastHit) < HitInterval)
            {
                continue;
            }
        }

        Info->CollisionInfo.HitActors.Add(HitActor, WorldTime);
        Info->CollisionInfo.HitCount++;
        Info->CollisionInfo.LastHitTime = WorldTime;
        Info->CollisionInfo.bHitThisFrame = true;

        const FVector HitLocation = HitActor->GetActorLocation();
        const FVector HitNormal = (HitLocation - Info->MoveInfo.Location).GetSafeNormal();
        FHitResult Hit(HitActor, nullptr, HitLocation, HitNormal);
        Hit.Location = HitLocation;
        Hit.ImpactPoint = HitLocation;
        Hit.TraceStart = Info->MoveInfo.Location;
        Hit.TraceEnd = Info->MoveInfo.Location;

        if (!Info->bIsSimple && Info->Entity && Info->Entity->GetLogicComponent())
        {
            if (!Info->Entity->GetLogicComponent()->FilterHit(*Info, Hit))
            {
                continue;
            }
        }

        const bool bDestroyed = HandleHitResult(*Info, HitActor, Hit, bApplyCollisionResponse);
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
        if (ChildData.ChildBulletID.IsNone() || ChildData.Count <= 0)
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
        ActionInfo.ChildBulletID = ChildData.ChildBulletID;
        ActionInfo.SpawnCount = ChildData.Count;
        ActionInfo.SpreadAngle = 0.0f;
        ActionInfo.SpawnLocationOffset = ChildData.SpawnLocationOffset;
        ActionInfo.bSpawnLocationOffsetInSpawnSpace = ChildData.bSpawnLocationOffsetInSpawnSpace;
        ActionInfo.SpawnRotationOffset = ChildData.SpawnRotationOffset;
        ActionInfo.InheritOwner = ChildData.bInheritOwner ? 1 : 0;
        ActionInfo.InheritTarget = ChildData.bInheritTarget ? 1 : 0;
        ActionInfo.InheritPayload = ChildData.bInheritPayload ? 1 : 0;
        EnqueueAction(ParentInfo.BulletId, ActionInfo);
    }
}

void UBulletController::SpawnChildBulletsFromLogic(
    const FBulletInfo& ParentInfo,
    FName ChildBulletID,
    int32 Count,
    float SpreadAngle,
    int32 InheritOwnerOverride,
    int32 InheritTargetOverride,
    int32 InheritPayloadOverride,
    const FVector& SpawnLocationOffset,
    bool bSpawnLocationOffsetInSpawnSpace,
    const FRotator& SpawnRotationOffset) const
{
    if (ChildBulletID.IsNone())
    {
        return;
    }

    const FVector Origin = ParentInfo.MoveInfo.Location;
    const FRotator BaseRot = ParentInfo.MoveInfo.Rotation;
    const FBulletDataChild* MatchingChild = FindChildEntry(ParentInfo, ChildBulletID);
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
        SpawnBullet(ChildParams, ChildBulletID, ChildId);
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

int32 UBulletController::FindBulletIdByActor(const AActor* Actor) const
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
    UE_LOG(LogBullet, Verbose, TEXT("HandleHitResult: BulletId=%d HitActor=%s"), Info.BulletId, HitActor ? *HitActor->GetName() : TEXT("None"));

    if (Info.Entity && Info.Entity->GetLogicComponent())
    {
        if (!Info.bIsSimple)
        {
            Info.Entity->GetLogicComponent()->HandleOnHit(Info, Hit);
        }

        if (HitActor && HitActor->IsA<ABulletActor>())
        {
            const int32 OtherBulletId = FindBulletIdByActor(HitActor);
            if (OtherBulletId != INDEX_NONE)
            {
                if (!Info.bIsSimple)
                {
                    Info.Entity->GetLogicComponent()->HandleOnHitBullet(Info, OtherBulletId);
                }
            }
        }
    }

    if (Info.Config.Interact.bEnableInteract && Info.Config.Interact.bAffectEnvironment && HitActor)
    {
        if (HitActor->GetClass()->ImplementsInterface(UBulletInteractInterface::StaticClass()))
        {
            IBulletInteractInterface::Execute_OnBulletInteract(HitActor, Info, Hit);
            UE_LOG(LogBullet, Verbose, TEXT("SceneInteract: BulletId=%d Actor=%s"), Info.BulletId, *HitActor->GetName());
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
            UE_LOG(LogBullet, Verbose, TEXT("Hit response: Destroy (BulletId=%d)"), Info.BulletId);
            RequestDestroyBullet(Info.BulletId, EBulletDestroyReason::Hit, true);
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
            UE_LOG(LogBullet, Verbose, TEXT("Hit response: Support->Destroy (BulletId=%d)"), Info.BulletId);
            RequestDestroyBullet(Info.BulletId, EBulletDestroyReason::Hit, true);
            return true;
        }
        break;
    case EBulletCollisionResponse::Pierce:
    default:
        break;
    }

    if (bHitLimitReached && !Info.bNeedDestroy)
    {
        UE_LOG(LogBullet, Verbose, TEXT("Hit limit reached: Destroy (BulletId=%d)"), Info.BulletId);
        RequestDestroyBullet(Info.BulletId, EBulletDestroyReason::Hit, true);
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

    TArray<int32> ToDestroy = Model->GetNeedDestroyBullets().Array();
    for (int32 BulletId : ToDestroy)
    {
        FBulletInfo* Info = Model->GetBullet(BulletId);
        if (!Info)
        {
            continue;
        }

        // Propagate parent destruction to existing child bullets (skip those spawned at destroy time).
        if (Info->DestroyWorldTime >= 0.0f)
        {
            TArray<int32> ChildIds;
            Model->GetChildBullets(BulletId, ChildIds);
            for (int32 ChildId : ChildIds)
            {
                FBulletInfo* ChildInfo = Model->GetBullet(ChildId);
                if (!ChildInfo || ChildInfo->bNeedDestroy)
                {
                    continue;
                }

                if (ChildInfo->SpawnWorldTime >= Info->DestroyWorldTime)
                {
                    continue;
                }

                RequestDestroyBullet(ChildId, EBulletDestroyReason::ParentDestroyed, false);
            }
        }

        if (ActionRunner)
        {
            ActionRunner->ClearBulletActions(BulletId);
        }

        if (Info->Actor)
        {
            ReleaseBulletActor(Info->Actor);
            Info->Actor = nullptr;
        }

        if (Info->Entity && BulletPool)
        {
            BulletPool->ReleaseEntity(Info->Entity);
        }

        Model->RemoveBullet(BulletId);

        UE_LOG(LogBullet, Verbose, TEXT("Bullet destroyed and recycled: Id=%d"), BulletId);
    }
}

FBulletInitParams UBulletController::BuildChildParams(const FBulletInfo& ParentInfo, const FTransform& ChildTransform, bool bInheritOwner, bool bInheritTarget, bool bInheritPayload) const
{
    FBulletInitParams Params;
    Params.Owner = bInheritOwner ? ParentInfo.InitParams.Owner : nullptr;
    Params.TargetActor = bInheritTarget ? ParentInfo.InitParams.TargetActor : nullptr;
    Params.TargetLocation = ParentInfo.InitParams.TargetLocation;
    Params.SpawnTransform = ChildTransform;
    Params.ContextId = ParentInfo.InitParams.ContextId;
    Params.AbilityId = ParentInfo.InitParams.AbilityId;
    if (bInheritPayload)
    {
        Params.Payload = ParentInfo.InitParams.Payload;
    }
    Params.ParentBulletId = ParentInfo.BulletId;
    Params.SyncType = ParentInfo.InitParams.SyncType;
    return Params;
}

const FBulletDataChild* UBulletController::FindChildEntry(const FBulletInfo& ParentInfo, FName ChildBulletID) const
{
    if (ChildBulletID.IsNone())
    {
        return nullptr;
    }

    for (const FBulletDataChild& Child : ParentInfo.Config.Children)
    {
        if (Child.ChildBulletID == ChildBulletID)
        {
            return &Child;
        }
    }

    return nullptr;
}

