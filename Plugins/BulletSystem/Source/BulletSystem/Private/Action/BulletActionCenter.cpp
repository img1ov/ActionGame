#include "Action/BulletActionCenter.h"
#include "Action/BulletActionBase.h"
#include "Action/BulletActions.h"
#include "BulletSystemLog.h"

void UBulletActionCenter::Initialize(UBulletController* InController)
{
    Controller = InController;
}

UBulletActionBase* UBulletActionCenter::AcquireAction(EBulletActionType ActionType)
{
    UClass* ActionClass = GetActionClass(ActionType);
    if (!ActionClass)
    {
        UE_LOG(LogBulletSystem, Warning, TEXT("BulletActionCenter: Missing action class for type %d"), static_cast<int32>(ActionType));
        return nullptr;
    }

    FBulletActionPool& Pool = ActionPools.FindOrAdd(ActionClass);
    if (Pool.Actions.Num() > 0)
    {
        UBulletActionBase* Action = Pool.Actions.Pop();
        return Action;
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
    ActionPools.FindOrAdd(ActionClass).Actions.Add(Action);
}

FBulletActionInfo UBulletActionCenter::AcquireActionInfo()
{
    if (ActionInfoPool.Num() > 0)
    {
        return ActionInfoPool.Pop();
    }

    return FBulletActionInfo();
}

void UBulletActionCenter::ReleaseActionInfo(const FBulletActionInfo& ActionInfo)
{
    ActionInfoPool.Add(ActionInfo);
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
