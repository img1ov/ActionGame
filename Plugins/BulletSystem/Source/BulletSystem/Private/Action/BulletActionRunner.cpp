#include "Action/BulletActionRunner.h"
#include "Action/BulletActionCenter.h"
#include "Action/BulletActionBase.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"

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

void UBulletActionRunner::EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo)
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

    for (auto& Pair : Model->GetMutableBulletMap())
    {
        FBulletInfo& Info = Pair.Value;
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

    if (FBulletActionList* Actions = PersistentActions.Find(BulletId))
    {
        for (UBulletActionBase* Action : Actions->Actions)
        {
            ActionCenter->ReleaseAction(Action);
        }
        PersistentActions.Remove(BulletId);
    }
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

        Action->Execute(Controller, BulletInfo, ActionInfo);

        if (Action->IsPersistent())
        {
            PersistentActions.FindOrAdd(BulletInfo.BulletId).Actions.Add(Action);
        }
        else
        {
            ActionCenter->ReleaseAction(Action);
        }
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
