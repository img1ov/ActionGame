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
    // Executes queued actions and drives persistent actions.
    void Initialize(UBulletController* InController, UBulletActionCenter* InActionCenter);

    void Pause();
    void Resume();

    void EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo) const;

    void Run(float DeltaSeconds);
    void AfterTick(float DeltaSeconds);

    void ClearBulletActions(int32 BulletId);

private:
    void ProcessActionList(FBulletInfo& BulletInfo, TArray<FBulletActionInfo>& ActionList);
    void TickPersistentActions(float DeltaSeconds);

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TObjectPtr<UBulletActionCenter> ActionCenter;

    UPROPERTY()
    TMap<int32, FBulletActionList> PersistentActions;

    EBulletActionRunnerState State = EBulletActionRunnerState::Idle;
};

