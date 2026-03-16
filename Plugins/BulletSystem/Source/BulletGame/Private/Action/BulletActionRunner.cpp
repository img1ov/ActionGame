#include "Action/BulletActionRunner.h"
#include "Action/BulletActionCenter.h"
#include "Action/BulletActionBase.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "BulletLogChannel.h"

void UBulletActionRunner::Initialize(UBulletController* InController, UBulletActionCenter* InActionCenter)
{
    Controller = InController;
    ActionCenter = InActionCenter;
}

void UBulletActionRunner::Pause()
{
    State = EBulletActionRunnerState::Pause;
}

void UBulletActionRunner::Resume()
{
    State = EBulletActionRunnerState::Idle;
}

void UBulletActionRunner::EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo) const
{
    if (!Controller)
    {
        return;
    }

    UBulletModel* Model = Controller->GetModel();
    if (!Model)
    {
        return;
    }

    FBulletInfo* Info = Model->GetBullet(BulletId);
    if (!Info)
    {
        return;
    }

    if (ClearingBullets.Contains(BulletId))
    {
        return;
    }

    if (State == EBulletActionRunnerState::Running)
    {
        Info->NextActionInfoList.Add(ActionInfo);
    }
    else
    {
        Info->ActionInfoList.Add(ActionInfo);
    }
}

void UBulletActionRunner::Run(float DeltaSeconds)
{
    if (!Controller || State == EBulletActionRunnerState::Pause)
    {
        return;
    }

    UBulletModel* Model = Controller->GetModel();
    if (!Model)
    {
        return;
    }

    State = EBulletActionRunnerState::Running;

    // Snapshot active bullet ids to avoid TMap rehash while actions spawn bullets.
    TArray<int32> BulletIds;
    BulletIds.Reserve(Model->GetBulletMap().Num());
    for (const auto& Pair : Model->GetBulletMap())
    {
        BulletIds.Add(Pair.Key);
    }

    for (int32 BulletId : BulletIds)
    {
        FBulletInfo* InfoPtr = Model->GetBullet(BulletId);
        if (!InfoPtr)
        {
            continue;
        }
        FBulletInfo& Info = *InfoPtr;
        if (Info.bNeedDestroy)
        {
            continue;
        }

        int32 SafetyCounter = 0;
        while ((Info.ActionInfoList.Num() > 0 || Info.NextActionInfoList.Num() > 0) && SafetyCounter++ < 64)
        {
            if (Info.ActionInfoList.Num() == 0 && Info.NextActionInfoList.Num() > 0)
            {
                Info.ActionInfoList = MoveTemp(Info.NextActionInfoList);
            }

            if (Info.ActionInfoList.Num() > 0)
            {
                TArray<FBulletActionInfo> CurrentActions = MoveTemp(Info.ActionInfoList);
                ProcessActionList(Info, CurrentActions);
            }
        }
    }

    TickPersistentActions(DeltaSeconds);

    State = EBulletActionRunnerState::Idle;
}

void UBulletActionRunner::AfterTick(float DeltaSeconds)
{
    for (auto& Pair : PersistentActions)
    {
        const int32 BulletId = Pair.Key;
        TArray<TObjectPtr<UBulletActionBase>>& Actions = Pair.Value.Actions;

        for (UBulletActionBase* Action : Actions)
        {
            if (!Action)
            {
                continue;
            }

            if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
            {
                if (FBulletInfo* Info = Model->GetBullet(BulletId))
                {
                    if (Info->bNeedDestroy)
                    {
                        continue;
                    }
                    Action->AfterTick(Controller, *Info, DeltaSeconds);
                }
            }
        }
    }
}

void UBulletActionRunner::ClearBulletActions(int32 BulletId)
{
    if (!ActionCenter)
    {
        return;
    }

    const EBulletActionRunnerState PrevState = State;
    State = EBulletActionRunnerState::ClearBullet;
    ClearingBullets.Add(BulletId);

    if (FBulletActionList* Actions = PersistentActions.Find(BulletId))
    {
        for (UBulletActionBase* Action : Actions->Actions)
        {
            ActionCenter->ReleaseAction(Action);
        }
        PersistentActions.Remove(BulletId);
    }

    if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
    {
        if (FBulletInfo* Info = Model->GetBullet(BulletId))
        {
            for (const FBulletActionInfo& ActionInfo : Info->ActionInfoList)
            {
                ActionCenter->ReleaseActionInfo(ActionInfo);
            }
            for (const FBulletActionInfo& ActionInfo : Info->NextActionInfoList)
            {
                ActionCenter->ReleaseActionInfo(ActionInfo);
            }
            Info->ActionInfoList.Reset();
            Info->NextActionInfoList.Reset();
        }
    }

    ClearingBullets.Remove(BulletId);
    State = PrevState;
}

void UBulletActionRunner::ProcessActionList(FBulletInfo& BulletInfo, TArray<FBulletActionInfo>& ActionList)
{
    if (!ActionCenter)
    {
        return;
    }

    for (const FBulletActionInfo& ActionInfo : ActionList)
    {
        UBulletActionBase* Action = ActionCenter->AcquireAction(ActionInfo.Type);
        if (!Action)
        {
            continue;
        }

        // Verbose log to trace action execution without spamming default logs.
        UE_LOG(LogBullet, Verbose, TEXT("Action Execute: BulletId=%d ActionType=%d"), BulletInfo.BulletId, static_cast<int32>(ActionInfo.Type));

        Action->Execute(Controller, BulletInfo, ActionInfo);

        if (Action->IsPersistent())
        {
            PersistentActions.FindOrAdd(BulletInfo.BulletId).Actions.Add(Action);
        }
        else
        {
            ActionCenter->ReleaseAction(Action);
        }

        ActionCenter->ReleaseActionInfo(ActionInfo);
    }
}

void UBulletActionRunner::TickPersistentActions(float DeltaSeconds)
{
    for (auto& Pair : PersistentActions)
    {
        const int32 BulletId = Pair.Key;
        TArray<TObjectPtr<UBulletActionBase>>& Actions = Pair.Value.Actions;

        for (UBulletActionBase* Action : Actions)
        {
            if (!Action)
            {
                continue;
            }

            if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
            {
                if (FBulletInfo* Info = Model->GetBullet(BulletId))
                {
                    if (Info->bNeedDestroy)
                    {
                        continue;
                    }
                    Action->Tick(Controller, *Info, DeltaSeconds);
                }
            }
        }
    }
}
