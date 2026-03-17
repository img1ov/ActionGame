#pragma once

// BulletSystem: BulletActionCenter.h
// Queued lifecycle action pipeline.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletActionCenter.generated.h"

class UBulletActionBase;
class UBulletController;

USTRUCT()
struct FBulletActionPool
{
    GENERATED_BODY()

    // Reusable action instances for a concrete action class (one pool per class).
    UPROPERTY()
    TArray<TObjectPtr<UBulletActionBase>> Actions;
};

UCLASS()
class BULLETGAME_API UBulletActionCenter : public UObject
{
    GENERATED_BODY()

public:
    // Factory + pool for action instances and lightweight action payload structs.
    // This reduces UObject churn and repeated allocations during burst spawns.
    void Initialize(UBulletController* InController);

    // Acquire an action instance for the given type. May reuse a pooled instance or allocate a new UObject.
    UBulletActionBase* AcquireAction(EBulletActionType ActionType);
    // Return an action instance to its per-class pool.
    void ReleaseAction(UBulletActionBase* Action);

    // Acquire a pooled action info struct (FBulletActionInfo). Always returned in a zeroed/default state.
    FBulletActionInfo AcquireActionInfo();
    // Return an action info struct to the pool.
    void ReleaseActionInfo(const FBulletActionInfo& ActionInfo);

private:
    UClass* GetActionClass(EBulletActionType ActionType) const;

    UPROPERTY()
    TObjectPtr<UBulletController> Controller;

    UPROPERTY()
    TMap<UClass*, FBulletActionPool> ActionPools;

    // Reusable value-type payloads. Stored as values to avoid separate heap allocations.
    UPROPERTY()
    TArray<FBulletActionInfo> ActionInfoPool;
};

