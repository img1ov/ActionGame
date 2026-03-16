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

    UPROPERTY()
    TArray<TObjectPtr<ABulletActor>> Actors;
};

UCLASS()
class BULLETGAME_API UBulletPool : public UObject
{
    GENERATED_BODY()

public:
    UBulletEntity* AcquireEntity(UObject* Outer);
    void ReleaseEntity(UBulletEntity* Entity);

private:
    UPROPERTY()
    TArray<TObjectPtr<UBulletEntity>> InactiveEntities;
};

UCLASS()
class BULLETGAME_API UBulletActorPool : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UWorld* InWorld, TSubclassOf<ABulletActor> InDefaultClass);

    ABulletActor* AcquireActor(TSubclassOf<ABulletActor> RequestedClass);
    void ReleaseActor(ABulletActor* Actor);

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

    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
};

UCLASS()
class BULLETGAME_API UBulletTraceElementPool : public UObject
{
    GENERATED_BODY()

public:
    FBulletTraceElement Acquire();
    void Release(const FBulletTraceElement& Element);

private:
    UPROPERTY()
    TArray<FBulletTraceElement> Pool;
};

