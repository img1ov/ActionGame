#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BulletPools.generated.h"

class UBulletEntity;
class ABulletActor;
class UWorld;

UCLASS()
class BULLETSYSTEM_API UBulletPool : public UObject
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
class BULLETSYSTEM_API UBulletActorPool : public UObject
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
    TArray<TObjectPtr<ABulletActor>> InactiveActors;
};

USTRUCT()
struct FBulletTraceElement
{
    GENERATED_BODY()

    FVector Start = FVector::ZeroVector;
    FVector End = FVector::ZeroVector;
};

UCLASS()
class BULLETSYSTEM_API UBulletTraceElementPool : public UObject
{
    GENERATED_BODY()

public:
    FBulletTraceElement Acquire();
    void Release(const FBulletTraceElement& Element);

private:
    UPROPERTY()
    TArray<FBulletTraceElement> Pool;
};
