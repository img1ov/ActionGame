#include "Action/BulletActionRunner.h"
#include "Action/BulletActionCenter.h"
#include "Action/BulletActionBase.h"
#include "Controller/BulletController.h"
#include "Model/BulletModel.h"
#include "Model/BulletInfo.h"
#include "BulletLogChannels.h"

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

void UBulletActionRunner::EnqueueAction(int32 InstanceId, const FBulletActionInfo& ActionInfo) const
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

    FBulletInfo* Info = Model->GetBullet(InstanceId);
    if (!Info)
    {
        return;
    }

    if (ClearingBullets.Contains(InstanceId))
    {
        return;
    }

    if (State == EBulletActionRunnerState::Running)
    {
        // Avoid mutating ActionInfoList while the runner is iterating it.
        // We'll swap NextActionInfoList into ActionInfoList once the current batch completes.
        Info->NextActionInfoList.Add(ActionInfo);
    }
    else
    {
        Info->ActionInfoList.Add(ActionInfo);
    }
}

void UBulletActionRunner::RunQueuedActionsForBullet(int32 InstanceId)
{
    if (!Controller || State == EBulletActionRunnerState::Pause)
    {
        return;
    }

    if (State == EBulletActionRunnerState::Running)
    {
        // Avoid re-entrancy. Bullets spawned during Run will be picked up next frame by the normal loop.
        return;
    }

    UBulletModel* Model = Controller->GetModel();
    if (!Model)
    {
        return;
    }

    FBulletInfo* InfoPtr = Model->GetBullet(InstanceId);
    if (!InfoPtr)
    {
        return;
    }

    FBulletInfo& Info = *InfoPtr;
    if (Info.bNeedDestroy)
    {
        return;
    }

    const EBulletActionRunnerState PrevState = State;
    State = EBulletActionRunnerState::Running;

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

    State = PrevState;
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

    // Snapshot active bullet ids to avoid iterator invalidation / TMap rehash while actions spawn or destroy bullets.
    TArray<int32> InstanceIds;
    InstanceIds.Reserve(Model->GetBulletMap().Num());
    for (const auto& Pair : Model->GetBulletMap())
    {
        InstanceIds.Add(Pair.Key);
    }

    for (int32 InstanceId : InstanceIds)
    {
        FBulletInfo* InfoPtr = Model->GetBullet(InstanceId);
        if (!InfoPtr)
        {
            continue;
        }
        FBulletInfo& Info = *InfoPtr;
        if (Info.bNeedDestroy)
        {
            continue;
        }

        // Safety valve: actions can enqueue more actions (including themselves). Cap the number of
        // action list drains per frame to prevent infinite loops from locking the game thread.
        int32 SafetyCounter = 0;
        while ((Info.ActionInfoList.Num() > 0 || Info.NextActionInfoList.Num() > 0) && SafetyCounter++ < 64)
        {
            if (Info.ActionInfoList.Num() == 0 && Info.NextActionInfoList.Num() > 0)
            {
                // Move deferred actions (queued during running) into the active queue.
                Info.ActionInfoList = MoveTemp(Info.NextActionInfoList);
            }

            if (Info.ActionInfoList.Num() > 0)
            {
                // Process a stable snapshot of the current queue; actions may enqueue additional work.
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
        const int32 InstanceId = Pair.Key;
        TArray<TObjectPtr<UBulletActionBase>>& Actions = Pair.Value.Actions;

        for (UBulletActionBase* Action : Actions)
        {
            if (!Action)
            {
                continue;
            }

            if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
            {
                if (FBulletInfo* Info = Model->GetBullet(InstanceId))
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

void UBulletActionRunner::ClearBulletActions(int32 InstanceId)
{
    if (!ActionCenter)
    {
        return;
    }

    const EBulletActionRunnerState PrevState = State;
    State = EBulletActionRunnerState::ClearBullet;
    ClearingBullets.Add(InstanceId);

    if (FBulletActionList* Actions = PersistentActions.Find(InstanceId))
    {
        for (UBulletActionBase* Action : Actions->Actions)
        {
            ActionCenter->ReleaseAction(Action);
        }
        PersistentActions.Remove(InstanceId);
    }

    if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
    {
        if (FBulletInfo* Info = Model->GetBullet(InstanceId))
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

    ClearingBullets.Remove(InstanceId);
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
        // Once a bullet is marked for destruction, do not execute any further queued actions for it.
        // We still need to release the queued ActionInfo payloads back to the pool.
        if (BulletInfo.bNeedDestroy)
        {
            ActionCenter->ReleaseActionInfo(ActionInfo);
            continue;
        }

        UBulletActionBase* Action = ActionCenter->AcquireAction(ActionInfo.Type);
        if (!Action)
        {
            continue;
        }

        // Verbose log to trace action execution without spamming default logs.
        UE_LOG(LogBullet, Verbose, TEXT("Action Execute: InstanceId=%d ActionType=%d"), BulletInfo.InstanceId, static_cast<int32>(ActionInfo.Type));

        Action->Execute(Controller, BulletInfo, ActionInfo);

        if (Action->IsPersistent())
        {
            PersistentActions.FindOrAdd(BulletInfo.InstanceId).Actions.Add(Action);
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
        const int32 InstanceId = Pair.Key;
        TArray<TObjectPtr<UBulletActionBase>>& Actions = Pair.Value.Actions;

        for (UBulletActionBase* Action : Actions)
        {
            if (!Action)
            {
                continue;
            }

            if (UBulletModel* Model = Controller ? Controller->GetModel() : nullptr)
            {
                if (FBulletInfo* Info = Model->GetBullet(InstanceId))
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
