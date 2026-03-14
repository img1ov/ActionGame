#include "Controller/BulletController.h"
#include "BulletSystemLog.h"
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
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Engine/EngineTypes.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void UBulletController::Initialize(UWorld* InWorld)
{
    World = InWorld;

    UGameInstance* GameInstance = InWorld ? InWorld->GetGameInstance() : nullptr;
    ConfigSubsystem = GameInstance ? GameInstance->GetSubsystem<UBulletConfigSubsystem>() : nullptr;

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    bEnableDebugDraw = Settings ? Settings->bEnableDebugDraw : false;

    Model = NewObject<UBulletModel>(this);
    BulletPool = NewObject<UBulletPool>(this);
    BulletActorPool = NewObject<UBulletActorPool>(this);
    TraceElementPool = NewObject<UBulletTraceElementPool>(this);

    ActionCenter = NewObject<UBulletActionCenter>(this);
    ActionCenter->Initialize(this);

    ActionRunner = NewObject<UBulletActionRunner>(this);
    ActionRunner->Initialize(this, ActionCenter);

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
}

void UBulletController::Shutdown()
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

void UBulletController::OnTick(float DeltaSeconds)
{
    if (!Model)
    {
        return;
    }

    if (ActionRunner)
    {
        ActionRunner->Pause();
    }

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

void UBulletController::OnAfterTick(float DeltaSeconds)
{
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

bool UBulletController::CreateBullet(const FBulletInitParams& InitParams, FName RowName, int32& OutBulletId, UBulletConfig* OverrideConfig)
{
    if (RowName.IsNone())
    {
        return false;
    }

    FBulletDataMain Data;
    bool bFound = false;

    if (OverrideConfig)
    {
        bFound = OverrideConfig->GetBulletData(RowName, Data);
    }
    else if (ConfigSubsystem)
    {
        bFound = ConfigSubsystem->GetBulletData(RowName, Data, InitParams.Owner);
    }

    if (!bFound)
    {
        UE_LOG(LogBulletSystem, Warning, TEXT("BulletController: Bullet row '%s' not found."), *RowName.ToString());
        return false;
    }

    return CreateBulletByData(InitParams, Data, OutBulletId);
}

bool UBulletController::CreateBulletByData(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutBulletId)
{
    if (!Model)
    {
        return false;
    }

    FBulletInfo* Info = Model->CreateBullet(BulletPool, InitParams, Data);
    if (!Info)
    {
        return false;
    }

    OutBulletId = Info->BulletId;
    EnqueueAction(OutBulletId, {EBulletActionType::InitBullet});
    return true;
}

void UBulletController::EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo)
{
    if (ActionRunner)
    {
        ActionRunner->EnqueueAction(BulletId, ActionInfo);
    }
}

void UBulletController::RequestDestroyBullet(int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren)
{
    FBulletActionInfo Info;
    Info.Type = EBulletActionType::DestroyBullet;
    Info.DestroyReason = Reason;
    Info.bSpawnChildren = bSpawnChildren;
    EnqueueAction(BulletId, Info);
}

void UBulletController::MarkBulletForDestroy(int32 BulletId)
{
    if (Model)
    {
        Model->MarkNeedDestroy(BulletId);
    }
}

bool UBulletController::SetCollisionEnabled(int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors)
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
        Info->CollisionInfo.LastHitTime = 0.0f;
    }

    return true;
}

bool UBulletController::ResetHitActors(int32 BulletId)
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
    Info->CollisionInfo.LastHitTime = 0.0f;
    return true;
}

int32 UBulletController::ApplyDamageToOverlaps(int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse)
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

    if (bResetHitActorsBefore)
    {
        Info->CollisionInfo.HitActors.Reset();
        Info->CollisionInfo.HitCount = 0;
        Info->CollisionInfo.LastHitTime = 0.0f;
    }

    if (Info->CollisionInfo.OverlapActors.Num() == 0)
    {
        return 0;
    }

    const float WorldTime = GetWorldTimeSeconds();
    int32 AppliedCount = 0;

    for (const TWeakObjectPtr<AActor>& ActorPtr : Info->CollisionInfo.OverlapActors)
    {
        AActor* HitActor = ActorPtr.Get();
        if (!HitActor)
        {
            continue;
        }

        if (Info->CollisionInfo.HitActors.Contains(HitActor))
        {
            continue;
        }

        Info->CollisionInfo.HitActors.Add(HitActor);
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

        const bool bDestroyed = HandleHitResult(*Info, HitActor, Hit, bApplyCollisionResponse);
        AppliedCount++;
        if (bDestroyed)
        {
            break;
        }
    }

    return AppliedCount;
}

void UBulletController::RequestSummonChildren(const FBulletInfo& ParentInfo)
{
    const FBulletDataChild& ChildData = ParentInfo.Config.Children;
    if (ChildData.Count <= 0 || ChildData.ChildRowName.IsNone())
    {
        return;
    }

    SpawnChildBulletsFromLogic(ParentInfo, ChildData.ChildRowName, ChildData.Count, ChildData.SpreadAngle);
}

void UBulletController::SpawnChildBulletsFromLogic(const FBulletInfo& ParentInfo, FName ChildRowName, int32 Count, float SpreadAngle)
{
    if (ChildRowName.IsNone() || Count <= 0)
    {
        return;
    }

    const FVector Origin = ParentInfo.MoveInfo.Location;
    const FRotator BaseRot = ParentInfo.MoveInfo.Rotation;

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float AngleOffset = (Count == 1) ? 0.0f : FMath::Lerp(-SpreadAngle, SpreadAngle, static_cast<float>(Index) / (Count - 1));
        const FRotator ChildRot = (BaseRot + FRotator(0.0f, AngleOffset, 0.0f));

        FBulletInitParams ChildParams = BuildChildParams(ParentInfo, FTransform(ChildRot, Origin));
        int32 ChildId = INDEX_NONE;
        CreateBullet(ChildParams, ChildRowName, ChildId);
    }
}

void UBulletController::SummonEntityFromConfig(const FBulletInfo& ParentInfo)
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

ABulletActor* UBulletController::AcquireBulletActor(const FBulletInfo& Info)
{
    if (!BulletActorPool)
    {
        return nullptr;
    }

    return BulletActorPool->AcquireActor(Info.Config.Render.ActorClass);
}

void UBulletController::ReleaseBulletActor(ABulletActor* Actor)
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

bool UBulletController::HandleHitResult(FBulletInfo& Info, AActor* HitActor, const FHitResult& Hit, bool bApplyCollisionResponse)
{
    if (Info.Entity && Info.Entity->GetLogicComponent())
    {
        Info.Entity->GetLogicComponent()->HandleOnHit(Info, Hit);

        if (HitActor && HitActor->IsA<ABulletActor>())
        {
            const int32 OtherBulletId = FindBulletIdByActor(HitActor);
            if (OtherBulletId != INDEX_NONE)
            {
                Info.Entity->GetLogicComponent()->HandleOnHitBullet(Info, OtherBulletId);
            }
        }
    }

    if (Info.Config.Children.bSpawnOnHit)
    {
        RequestSummonChildren(Info);
    }

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
                Info.Entity->GetLogicComponent()->HandleOnRebound(Info, Hit);
            }
        }
        break;
    case EBulletCollisionResponse::Pierce:
    default:
        break;
    }

    return false;
}

void UBulletController::FlushDestroyedBullets()
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
    }
}

FBulletInitParams UBulletController::BuildChildParams(const FBulletInfo& ParentInfo, const FTransform& ChildTransform) const
{
    FBulletInitParams Params;
    Params.Owner = ParentInfo.Config.Children.bInheritOwner ? ParentInfo.InitParams.Owner : nullptr;
    Params.TargetActor = ParentInfo.Config.Children.bInheritTarget ? ParentInfo.InitParams.TargetActor : nullptr;
    Params.TargetLocation = ParentInfo.InitParams.TargetLocation;
    Params.SpawnTransform = ChildTransform;
    Params.ContextId = ParentInfo.InitParams.ContextId;
    Params.SkillId = ParentInfo.InitParams.SkillId;
    Params.ParentBulletId = ParentInfo.BulletId;
    Params.SyncType = ParentInfo.InitParams.SyncType;
    return Params;
}
