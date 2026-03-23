#pragma once

// BulletSystem: BulletPools.h
// Object pools and reuse helpers.

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BulletPools.generated.h"

class UBulletEntity;
class ABulletActor;
class UWorld;

USTRUCT()
struct FBulletActorPoolBucket
{
    GENERATED_BODY()

    // Inactive (pooled) actors of a specific class.
    UPROPERTY()
    TArray<TObjectPtr<ABulletActor>> Actors;
};

UCLASS()
class BULLETSYSTEM_API UBulletPool : public UObject
{
    GENERATED_BODY()

public:
    // Acquire a lightweight bullet entity. Entities own per-bullet components (logic/budget/actor binding).
    UBulletEntity* AcquireEntity(UObject* Outer);
    // Return an entity to the pool and reset its components.
    void ReleaseEntity(UBulletEntity* Entity);
    // Clear pooled entities (typically on world teardown / PIE end).
    void Clear();

private:
    // Entities are reused to avoid per-bullet allocation churn during burst spawns.
    UPROPERTY()
    TArray<TObjectPtr<UBulletEntity>> InactiveEntities;
};

UCLASS()
class BULLETSYSTEM_API UBulletActorPool : public UObject
{
    GENERATED_BODY()

public:
    // Initialize the pool with a world context and an optional default actor class.
    void Initialize(UWorld* InWorld, TSubclassOf<ABulletActor> InDefaultClass);

    // Acquire a render actor for a bullet. The pool is partitioned per class to avoid mismatched setups.
    ABulletActor* AcquireActor(TSubclassOf<ABulletActor> RequestedClass);
    // Release a render actor back into the pool (detaches, hides, resets state).
    void ReleaseActor(ABulletActor* Actor);

    // Destroy all pooled actors and clear buckets (typically on world teardown).
    void Clear();

private:
    UPROPERTY()
    TWeakObjectPtr<UWorld> World;

    UPROPERTY()
    TSubclassOf<ABulletActor> DefaultActorClass;

    UPROPERTY()
    TMap<UClass*, FBulletActorPoolBucket> InactiveActorsByClass;
};

USTRUCT()
struct FBulletTraceElement
{
    GENERATED_BODY()

    // Start/end points for traces. Stored in a value pool to reduce allocations in tight loops.
    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
};

UCLASS()
class BULLETSYSTEM_API UBulletTraceElementPool : public UObject
{
    GENERATED_BODY()

public:
    // Acquire a trace element from the pool (value type).
    FBulletTraceElement Acquire();
    // Return a trace element to the pool (value type).
    void Release(const FBulletTraceElement& Element);
    // Clear pooled elements (typically on world teardown / PIE end).
    void Clear();

private:
    // Value-type pool; avoids heap allocations for per-bullet temporary structs.
    UPROPERTY()
    TArray<FBulletTraceElement> Pool;
};

