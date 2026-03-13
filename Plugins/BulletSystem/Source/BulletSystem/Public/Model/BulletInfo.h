#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "BulletSystemTypes.h"
#include "BulletInfo.generated.h"

class UBulletEntity;
class ABulletActor;
class UNiagaraComponent;
class UCurveVector;

USTRUCT(BlueprintType)
struct FBulletMoveInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector LastLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Acceleration = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bInitialized = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bAttachToOwner = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float OrbitAngle = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector OrbitCenter = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    TObjectPtr<UCurveVector> CustomMoveCurve = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float CustomMoveDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float CustomMoveElapsed = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletCollisionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    int32 HitCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    float LastHitTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    bool bHitThisFrame = false;

    TArray<TWeakObjectPtr<AActor>> HitActors;
};

USTRUCT(BlueprintType)
struct FBulletEffectInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    TObjectPtr<UNiagaraComponent> NiagaraComponent = nullptr;
};

USTRUCT(BlueprintType)
struct FBulletChildInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    int32 SpawnedCount = 0;
};

USTRUCT(BlueprintType)
struct FBulletRayInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray")
    FVector TraceStart = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray")
    FVector TraceEnd = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FBulletInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    int32 BulletId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletInitParams InitParams;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletDataMain Config;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TObjectPtr<UBulletEntity> Entity = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TObjectPtr<ABulletActor> Actor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletMoveInfo MoveInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletCollisionInfo CollisionInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletEffectInfo EffectInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletChildInfo ChildInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletRayInfo RayInfo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bIsInit = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bNeedDestroy = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float LiveTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float PendingDestroyDelay = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float TimeScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FVector Size = FVector::OneVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bFrozen = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float FrozenUntilTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FGameplayTagContainer Tags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TArray<FBulletActionInfo> ActionInfoList;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TArray<FBulletActionInfo> NextActionInfoList;
};
