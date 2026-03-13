#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletController.generated.h"

class UBulletModel;
class UBulletActionRunner;
class UBulletActionCenter;
class UBulletSystemBase;
class UBulletMoveSystem;
class UBulletCollisionSystem;
class UBulletPool;
class UBulletActorPool;
class UBulletTraceElementPool;
class UBulletConfigSubsystem;
class UBulletConfig;
class ABulletActor;
class UWorld;
struct FBulletInfo;

UCLASS()
class BULLETSYSTEM_API UBulletController : public UObject
{
    GENERATED_BODY()

public:
    // Central orchestrator for bullet lifecycle, tick order, and pooling.
    void Initialize(UWorld* InWorld);
    void Shutdown();

    void OnTick(float DeltaSeconds);
    void OnAfterTick(float DeltaSeconds);

    bool CreateBullet(const FBulletInitParams& InitParams, FName RowName, int32& OutBulletId, UBulletConfig* OverrideConfig = nullptr);
    bool CreateBulletByData(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutBulletId);

    void EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo);
    void RequestDestroyBullet(int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren);
    void MarkBulletForDestroy(int32 BulletId);

    void RequestSummonChildren(const FBulletInfo& ParentInfo);
    void SpawnChildBulletsFromLogic(const FBulletInfo& ParentInfo, FName ChildRowName, int32 Count, float SpreadAngle);
    void SummonEntityFromConfig(const FBulletInfo& ParentInfo);

    ABulletActor* AcquireBulletActor(const FBulletInfo& Info);
    void ReleaseBulletActor(ABulletActor* Actor);

    int32 FindBulletIdByActor(const AActor* Actor) const;

    UBulletModel* GetModel() const;
    UWorld* GetWorld() const;
    float GetWorldTimeSeconds() const;
    bool IsDebugDrawEnabled() const;

private:
    void FlushDestroyedBullets();
    FBulletInitParams BuildChildParams(const FBulletInfo& ParentInfo, const FTransform& ChildTransform) const;

    UPROPERTY()
    TWeakObjectPtr<UWorld> World;

    UPROPERTY()
    TObjectPtr<UBulletModel> Model;

    UPROPERTY()
    TObjectPtr<UBulletActionCenter> ActionCenter;

    UPROPERTY()
    TObjectPtr<UBulletActionRunner> ActionRunner;

    UPROPERTY()
    TObjectPtr<UBulletPool> BulletPool;

    UPROPERTY()
    TObjectPtr<UBulletActorPool> BulletActorPool;

    UPROPERTY()
    TObjectPtr<UBulletTraceElementPool> TraceElementPool;

    UPROPERTY()
    TObjectPtr<UBulletMoveSystem> MoveSystem;

    UPROPERTY()
    TObjectPtr<UBulletCollisionSystem> CollisionSystem;

    UPROPERTY()
    TObjectPtr<UBulletConfigSubsystem> ConfigSubsystem;

    bool bEnableDebugDraw = false;
};
