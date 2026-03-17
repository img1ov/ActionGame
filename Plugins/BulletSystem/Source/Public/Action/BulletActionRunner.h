#pragma once

// BulletSystem: BulletActionRunner.h
// Queued lifecycle action pipeline.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletActionRunner.generated.h"

class UBulletActionCenter;
class UBulletActionBase;
class UBulletController;
class UBulletModel;
struct FBulletInfo;

USTRUCT()
struct FBulletActionList
{
    GENERATED_BODY()

    // Persistent actions are instantiated UObject actions that tick every frame for a given bullet
    // (e.g. timers, continuous effects, delayed operations).
    UPROPERTY()
    TArray<TObjectPtr<UBulletActionBase>> Actions;
};

UENUM()
enum class EBulletActionRunnerState : uint8
{
    Idle,
    Pause,
    Running,
    ClearBullet
};

UCLASS()
class BULLETGAME_API UBulletActionRunner : public UObject
{
    GENERATED_BODY()

public:
    // Executes queued (one-shot) actions and ticks persistent actions.
    // The runner provides deterministic ordering:
    // - Per-frame systems (move/collision) run first.
    // - Actions are then drained in FIFO order per bullet.
    void Initialize(UBulletController* InController, UBulletActionCenter* InActionCenter);

    // Temporarily stop processing actions (used while systems tick to prevent re-entrancy).
    void Pause();
    // Resume processing actions after systems have finished.
    void Resume();

    // Enqueue a one-shot action. If called while running, actions are deferred into FBulletInfo::NextActionInfoList.
    void EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo) const;

    // Drain queued actions for all bullets and then tick persistent actions.
    void Run(float DeltaSeconds);
    // Optional post-tick stage for persistent actions (cleanup, late updates).
    void AfterTick(float DeltaSeconds);

    // Clears both queued and persistent actions for a bullet (called during bullet destruction).
    void ClearBulletActions(int32 BulletId);

private:
    // Executes a batch of actions for a single bullet. One-shot actions are released back to the action center pool.
    void ProcessActionList(FBulletInfo& BulletInfo, TArray<FBulletActionInfo>& ActionList);
    // Ticks all persistent actions across all active bullets.
    void TickPersistentActions(float DeltaSeconds);

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TObjectPtr<UBulletActionCenter> ActionCenter;

    UPROPERTY()
    TMap<int32, FBulletActionList> PersistentActions;

    // Bullets currently being cleared; prevents new actions from being queued during teardown.
    TSet<int32> ClearingBullets;

    // Simple runner state machine to control where new actions are enqueued.
    EBulletActionRunnerState State = EBulletActionRunnerState::Idle;
};

