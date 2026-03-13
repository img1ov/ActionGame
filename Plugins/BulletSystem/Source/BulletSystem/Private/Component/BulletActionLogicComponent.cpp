#include "Component/BulletActionLogicComponent.h"
#include "Logic/BulletLogicData.h"
#include "Logic/BulletLogicControllers.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "BulletSystemLog.h"

void UBulletActionLogicComponent::Initialize(UBulletController* InController, UBulletEntity* InEntity, const FBulletDataExecution& ExecutionData)
{
    Controller = InController;
    Entity = InEntity;

    Reset();

    for (const TSoftObjectPtr<UBulletLogicData>& DataAssetPtr : ExecutionData.LogicDataList)
    {
        if (!DataAssetPtr.ToSoftObjectPath().IsValid())
        {
            continue;
        }

        UBulletLogicData* DataAsset = DataAssetPtr.Get();
        if (!DataAsset)
        {
            FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
            DataAsset = Cast<UBulletLogicData>(Streamable.LoadSynchronous(DataAssetPtr.ToSoftObjectPath()));
        }

        if (!DataAsset)
        {
            UE_LOG(LogBulletSystem, Warning, TEXT("BulletActionLogicComponent: Failed to load logic data asset."));
            continue;
        }

        TSubclassOf<UBulletLogicController> ControllerClass = DataAsset->ControllerClass;
        if (!ControllerClass)
        {
            UE_LOG(LogBulletSystem, Warning, TEXT("BulletActionLogicComponent: Logic data missing controller class."));
            continue;
        }

        UBulletLogicController* LogicController = NewObject<UBulletLogicController>(this, ControllerClass);
        LogicController->Initialize(InController, InEntity, DataAsset);
        AddController(LogicController, DataAsset->Trigger);
    }
}

void UBulletActionLogicComponent::Reset()
{
    OnBeginControllers.Reset();
    OnHitControllers.Reset();
    OnDestroyControllers.Reset();
    OnReboundControllers.Reset();
    OnSupportControllers.Reset();
    ReplaceMoveControllers.Reset();
    OnHitBulletControllers.Reset();
    TickControllers.Reset();
}

void UBulletActionLogicComponent::HandleOnBegin(FBulletInfo& BulletInfo)
{
    for (UBulletLogicController* Logic : OnBeginControllers)
    {
        if (Logic)
        {
            Logic->OnBegin(BulletInfo);
        }
    }
}

void UBulletActionLogicComponent::HandleOnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    for (UBulletLogicController* Logic : OnHitControllers)
    {
        if (Logic)
        {
            Logic->OnHit(BulletInfo, Hit);
        }
    }
}

void UBulletActionLogicComponent::HandleOnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason)
{
    for (UBulletLogicController* Logic : OnDestroyControllers)
    {
        if (Logic)
        {
            Logic->OnDestroy(BulletInfo, Reason);
        }
    }
}

void UBulletActionLogicComponent::HandleOnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    for (UBulletLogicController* Logic : OnReboundControllers)
    {
        if (Logic)
        {
            Logic->OnRebound(BulletInfo, Hit);
        }
    }
}

void UBulletActionLogicComponent::HandleOnSupport(FBulletInfo& BulletInfo)
{
    for (UBulletLogicController* Logic : OnSupportControllers)
    {
        if (Logic)
        {
            Logic->OnSupport(BulletInfo);
        }
    }
}

void UBulletActionLogicComponent::HandleOnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    for (UBulletLogicController* Logic : OnHitBulletControllers)
    {
        if (Logic)
        {
            Logic->OnHitBullet(BulletInfo, OtherBulletId);
        }
    }
}

void UBulletActionLogicComponent::TickLogic(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    for (UBulletLogicController* Logic : TickControllers)
    {
        if (Logic)
        {
            Logic->Tick(BulletInfo, DeltaSeconds);
        }
    }
}

bool UBulletActionLogicComponent::HandleReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    bool bReplaced = false;
    for (UBulletLogicController* Logic : ReplaceMoveControllers)
    {
        if (Logic)
        {
            bReplaced |= Logic->ReplaceMove(BulletInfo, DeltaSeconds);
        }
    }
    return bReplaced;
}

void UBulletActionLogicComponent::AddController(UBulletLogicController* Logic, EBulletLogicTrigger Trigger)
{
    if (!Logic)
    {
        return;
    }

    switch (Trigger)
    {
    case EBulletLogicTrigger::OnBegin:
        OnBeginControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::OnHit:
        OnHitControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::OnDestroy:
        OnDestroyControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::OnRebound:
        OnReboundControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::OnSupport:
        OnSupportControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::ReplaceMove:
        ReplaceMoveControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::OnHitBullet:
        OnHitBulletControllers.Add(Logic);
        break;
    case EBulletLogicTrigger::Tick:
        TickControllers.Add(Logic);
        break;
    default:
        break;
    }
}
