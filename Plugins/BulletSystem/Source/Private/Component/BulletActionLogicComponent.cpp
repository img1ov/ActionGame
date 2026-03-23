// BulletSystem: BulletActionLogicComponent.cpp
// Components used by bullet entities/actors.
#include "Component/BulletActionLogicComponent.h"
#include "Logic/BulletLogicData.h"
#include "Logic/BulletLogicController.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "BulletLogChannels.h"

void UBulletActionLogicComponent::Initialize(UBulletController* InController, UBulletEntity* InEntity, const FBulletDataExecution& ExecutionData)
{
    Controller = InController;
    Entity = InEntity;

    OnBeginControllers.Reset();
    OnHitControllers.Reset();
    OnDestroyControllers.Reset();
    OnReboundControllers.Reset();
    OnSupportControllers.Reset();
    ReplaceMoveControllers.Reset();
    OnHitBulletControllers.Reset();
    TickControllers.Reset();
    AllControllers.Reset();

    if (ExecutionData.LogicDataList.Num() == 0)
    {
        return;
    }

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

    for (const TSoftObjectPtr<UBulletLogicData>& LogicDataPtr : ExecutionData.LogicDataList)
    {
        if (!LogicDataPtr.ToSoftObjectPath().IsValid())
        {
            UE_LOG(LogBullet, VeryVerbose, TEXT("BulletLogic: skip invalid LogicData soft path."));
            continue;
        }

        UBulletLogicData* LogicData = LogicDataPtr.Get();
        if (!LogicData)
        {
            LogicData = Cast<UBulletLogicData>(Streamable.LoadSynchronous(LogicDataPtr.ToSoftObjectPath()));
        }

        if (!LogicData || !LogicData->ControllerClass)
        {
            UE_LOG(LogBullet, Warning, TEXT("BulletLogic: failed to load LogicData or ControllerClass missing. Asset=%s"),
                *LogicDataPtr.ToSoftObjectPath().ToString());
            continue;
        }

        UBulletLogicController* LogicController = NewObject<UBulletLogicController>(this, LogicData->ControllerClass);
        if (!LogicController)
        {
            UE_LOG(LogBullet, Warning, TEXT("BulletLogic: failed to create controller. Class=%s Asset=%s"),
                *GetNameSafe(LogicData->ControllerClass.Get()),
                *LogicDataPtr.ToSoftObjectPath().ToString());
            continue;
        }

        LogicController->Initialize(Controller, Entity, LogicData);
        AddController(LogicController, LogicData->Trigger);
    }
}

void UBulletActionLogicComponent::Reset()
{
    Super::Reset();

    Controller = nullptr;

    OnBeginControllers.Reset();
    OnHitControllers.Reset();
    OnDestroyControllers.Reset();
    OnReboundControllers.Reset();
    OnSupportControllers.Reset();
    ReplaceMoveControllers.Reset();
    OnHitBulletControllers.Reset();
    TickControllers.Reset();
    AllControllers.Reset();
}

void UBulletActionLogicComponent::HandleOnBegin(FBulletInfo& BulletInfo)
{
    for (UBulletLogicController* LogicController : OnBeginControllers)
    {
        if (LogicController)
        {
            LogicController->OnBegin(BulletInfo);
        }
    }
}

void UBulletActionLogicComponent::HandleOnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    for (UBulletLogicController* LogicController : OnHitControllers)
    {
        if (LogicController)
        {
            LogicController->OnHit(BulletInfo, Hit);
        }
    }
}

void UBulletActionLogicComponent::HandleOnDestroy(FBulletInfo& BulletInfo)
{
    for (UBulletLogicController* LogicController : OnDestroyControllers)
    {
        if (LogicController)
        {
            LogicController->OnDestroy(BulletInfo);
        }
    }
}

void UBulletActionLogicComponent::HandleOnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    for (UBulletLogicController* LogicController : OnReboundControllers)
    {
        if (LogicController)
        {
            LogicController->OnRebound(BulletInfo, Hit);
        }
    }
}

void UBulletActionLogicComponent::HandleOnSupport(FBulletInfo& BulletInfo)
{
    for (UBulletLogicController* LogicController : OnSupportControllers)
    {
        if (LogicController)
        {
            LogicController->OnSupport(BulletInfo);
        }
    }
}

void UBulletActionLogicComponent::HandleOnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    for (UBulletLogicController* LogicController : OnHitBulletControllers)
    {
        if (LogicController)
        {
            LogicController->OnHitBullet(BulletInfo, OtherBulletId);
        }
    }
}

void UBulletActionLogicComponent::TickLogic(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    for (UBulletLogicController* LogicController : TickControllers)
    {
        if (LogicController)
        {
            LogicController->Tick(BulletInfo, DeltaSeconds);
        }
    }
}

bool UBulletActionLogicComponent::HandleReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    bool bReplaced = false;
    for (UBulletLogicController* LogicController : ReplaceMoveControllers)
    {
        if (LogicController)
        {
            bReplaced |= LogicController->ReplaceMove(BulletInfo, DeltaSeconds);
        }
    }
    return bReplaced;
}

void UBulletActionLogicComponent::HandlePreMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    for (UBulletLogicController* LogicController : AllControllers)
    {
        if (LogicController)
        {
            LogicController->OnPreMove(BulletInfo, DeltaSeconds);
        }
    }
}

void UBulletActionLogicComponent::HandlePostMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    for (UBulletLogicController* LogicController : AllControllers)
    {
        if (LogicController)
        {
            LogicController->OnPostMove(BulletInfo, DeltaSeconds);
        }
    }
}

void UBulletActionLogicComponent::HandlePreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    for (UBulletLogicController* LogicController : AllControllers)
    {
        if (LogicController)
        {
            LogicController->OnPreCollision(BulletInfo, Hits);
        }
    }
}

void UBulletActionLogicComponent::HandlePostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    for (UBulletLogicController* LogicController : AllControllers)
    {
        if (LogicController)
        {
            LogicController->OnPostCollision(BulletInfo, Hits);
        }
    }
}

bool UBulletActionLogicComponent::FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    for (UBulletLogicController* LogicController : AllControllers)
    {
        if (LogicController && !LogicController->FilterHit(BulletInfo, Hit))
        {
            return false;
        }
    }

    return true;
}

void UBulletActionLogicComponent::AddController(UBulletLogicController* LogicController, EBulletLogicTrigger Trigger)
{
    if (!LogicController)
    {
        return;
    }

    AllControllers.Add(LogicController);

    switch (Trigger)
    {
    case EBulletLogicTrigger::OnBegin:
        OnBeginControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::OnHit:
        OnHitControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::OnDestroy:
        OnDestroyControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::OnRebound:
        OnReboundControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::OnSupport:
        OnSupportControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::ReplaceMove:
        ReplaceMoveControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::OnHitBullet:
        OnHitBulletControllers.Add(LogicController);
        break;
    case EBulletLogicTrigger::Tick:
        TickControllers.Add(LogicController);
        break;
    default:
        break;
    }
}
