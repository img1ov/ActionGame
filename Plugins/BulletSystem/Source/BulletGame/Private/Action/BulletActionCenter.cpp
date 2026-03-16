// BulletSystem: BulletActionCenter.cpp
// Queued lifecycle action pipeline.
#include "Action/BulletActionCenter.h"
#include "Action/BulletActions.h"
#include "Action/BulletActionBase.h"
#include "Controller/BulletController.h"

void UBulletActionCenter::Initialize(UBulletController* InController)
{
    Controller = InController;
    ActionPools.Empty();
    ActionInfoPool.Empty();
}

UBulletActionBase* UBulletActionCenter::AcquireAction(EBulletActionType ActionType)
{
    UClass* ActionClass = GetActionClass(ActionType);
    if (!ActionClass)
    {
        return nullptr;
    }

    if (FBulletActionPool* Pool = ActionPools.Find(ActionClass))
    {
        if (Pool->Actions.Num() > 0)
        {
            return Pool->Actions.Pop(EAllowShrinking::No);
        }
    }

    return NewObject<UBulletActionBase>(this, ActionClass);
}

void UBulletActionCenter::ReleaseAction(UBulletActionBase* Action)
{
    if (!Action)
    {
        return;
    }

    UClass* ActionClass = Action->GetClass();
    if (!ActionClass)
    {
        return;
    }

    ActionPools.FindOrAdd(ActionClass).Actions.Add(Action);
}

FBulletActionInfo UBulletActionCenter::AcquireActionInfo()
{
    if (ActionInfoPool.Num() > 0)
    {
        return ActionInfoPool.Pop(EAllowShrinking::No);
    }

    return FBulletActionInfo();
}

void UBulletActionCenter::ReleaseActionInfo(const FBulletActionInfo& ActionInfo)
{
    (void)ActionInfo;
}

UClass* UBulletActionCenter::GetActionClass(EBulletActionType ActionType) const
{
    switch (ActionType)
    {
    case EBulletActionType::InitBullet:
        return UBulletActionInitBullet::StaticClass();
    case EBulletActionType::InitHit:
        return UBulletActionInitHit::StaticClass();
    case EBulletActionType::InitMove:
        return UBulletActionInitMove::StaticClass();
    case EBulletActionType::InitCollision:
        return UBulletActionInitCollision::StaticClass();
    case EBulletActionType::InitRender:
        return UBulletActionInitRender::StaticClass();
    case EBulletActionType::TimeScale:
        return UBulletActionTimeScale::StaticClass();
    case EBulletActionType::AfterInit:
        return UBulletActionAfterInit::StaticClass();
    case EBulletActionType::Child:
        return UBulletActionChild::StaticClass();
    case EBulletActionType::SummonBullet:
        return UBulletActionSummonBullet::StaticClass();
    case EBulletActionType::SummonEntity:
        return UBulletActionSummonEntity::StaticClass();
    case EBulletActionType::DestroyBullet:
        return UBulletActionDestroyBullet::StaticClass();
    case EBulletActionType::DelayDestroyBullet:
        return UBulletActionDelayDestroyBullet::StaticClass();
    case EBulletActionType::SceneInteract:
        return UBulletActionSceneInteract::StaticClass();
    case EBulletActionType::UpdateEffect:
        return UBulletActionUpdateEffect::StaticClass();
    case EBulletActionType::UpdateLiveTime:
        return UBulletActionUpdateLiveTime::StaticClass();
    case EBulletActionType::UpdateAttackerFrozen:
        return UBulletActionUpdateAttackerFrozen::StaticClass();
    default:
        return nullptr;
    }
}
