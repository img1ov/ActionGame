#pragma once

// BulletSystem: BulletController.h
// Runtime controller layer and orchestration.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemTypes.h"
#include "BulletController.generated.h"

// Runtime orchestration of bullets: owns model/pools, drives tick order,
// and funnels all lifecycle requests through the action pipeline.
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
class AActor;
class UWorld;
struct FBulletInfo;
struct FHitResult;

enum class EBulletChildSpawnTrigger : uint8
{
    OnCreate,
    OnHit,
    OnDestroy
};

UCLASS()
class BULLETGAME_API UBulletController : public UObject
{
    GENERATED_BODY()

public:
    // Initialize all runtime subsystems (model, pools, action runner, move/collision).
    void Initialize(UWorld* InWorld);
    // Release pooled resources and clear all bullets.
    void Shutdown() const;

    // Main tick: move/collision systems first, then action runner.
    void OnTick(float DeltaSeconds) const;
    // Post-tick stage used for cleanup and deferred destruction.
    void OnAfterTick(float DeltaSeconds) const;

    // Spawn a bullet by BulletID from config asset or subsystem.
    bool SpawnBullet(const FBulletInitParams& InitParams, FName BulletID, int32& OutBulletId, const UBulletConfig* OverrideConfig = nullptr) const;
    // Spawn using a pre-resolved config struct.
    bool SpawnBulletByData(const FBulletInitParams& InitParams, const FBulletDataMain& Data, int32& OutBulletId) const;

    // Queue a lifecycle action for deterministic execution order.
    void EnqueueAction(int32 BulletId, const FBulletActionInfo& ActionInfo) const;
    // Request bullet destruction through the action pipeline.
    void RequestDestroyBullet(int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren) const;
    // Mark a bullet for destruction at end of tick.
    void MarkBulletForDestroy(int32 BulletId) const;

    // Runtime collision toggles and hit-cache management.
    bool SetCollisionEnabled(int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors) const;
    bool ResetHitActors(int32 BulletId) const;
    // Manual-hit trigger: process stored overlaps as hits (fires OnHit logic chain, interact, collision response).
    int32 ProcessManualHits(int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse) const;

    // Resolve a hit and apply response (logic hooks, interact, destroy/bounce/support).
    bool HandleHitResult(FBulletInfo& Info, AActor* HitActor, const FHitResult& Hit, bool bApplyCollisionResponse) const;

    // Parent/child relationship helpers for hierarchical bullets.
    void GetChildBulletIds(int32 ParentBulletId, TArray<int32>& OutChildren) const;
    int32 GetParentBulletId(int32 ChildBulletId) const;

    // Child/summon helpers used by actions and logic.
    void RequestSummonChildren(const FBulletInfo& ParentInfo, EBulletChildSpawnTrigger Trigger) const;
    void SpawnChildBulletsFromLogic(
        const FBulletInfo& ParentInfo,
        FName ChildBulletID,
        int32 Count,
        float SpreadAngle,
        int32 InheritOwnerOverride = -1,
        int32 InheritTargetOverride = -1,
        int32 InheritPayloadOverride = -1,
        const FVector& SpawnLocationOffset = FVector::ZeroVector,
        bool bSpawnLocationOffsetInSpawnSpace = true,
        const FRotator& SpawnRotationOffset = FRotator::ZeroRotator) const;
    void SummonEntityFromConfig(const FBulletInfo& ParentInfo) const;

    // Acquire/release pooled render actor.
    ABulletActor* AcquireBulletActor(const FBulletInfo& Info) const;
    void ReleaseBulletActor(ABulletActor* Actor) const;

    // Reverse lookup from render actor to bullet id.
    int32 FindBulletIdByActor(const AActor* Actor) const;

    UBulletModel* GetModel() const;
    UBulletTraceElementPool* GetTraceElementPool() const;
    virtual UWorld* GetWorld() const override;
    float GetWorldTimeSeconds() const;
    bool IsDebugDrawEnabled() const;

private:
    // Finalize pending destroys and release pooled objects.
    void FlushDestroyedBullets() const;
    // Build child init params (owner/target inheritance).
    FBulletInitParams BuildChildParams(const FBulletInfo& ParentInfo, const FTransform& ChildTransform, bool bInheritOwner, bool bInheritTarget, bool bInheritPayload) const;
    const FBulletDataChild* FindChildEntry(const FBulletInfo& ParentInfo, FName ChildBulletID) const;

    UPROPERTY()
    TWeakObjectPtr<UWorld> World;

    UPROPERTY()
    TObjectPtr<UBulletModel> Model;

    // Action center is responsible for pooling/creating UBulletActionBase instances and FBulletActionInfo payloads.
    UPROPERTY()
    TObjectPtr<UBulletActionCenter> ActionCenter;

    // Runner drains per-bullet action queues and ticks persistent actions.
    UPROPERTY()
    TObjectPtr<UBulletActionRunner> ActionRunner;

    // Pools for per-bullet entity state, render actors, and temporary trace structs.
    UPROPERTY()
    TObjectPtr<UBulletPool> BulletPool;

    UPROPERTY()
    TObjectPtr<UBulletActorPool> BulletActorPool;

    UPROPERTY()
    TObjectPtr<UBulletTraceElementPool> TraceElementPool;

    // High-frequency per-frame systems. These run before action execution.
    UPROPERTY()
    TObjectPtr<UBulletMoveSystem> MoveSystem;

    UPROPERTY()
    TObjectPtr<UBulletCollisionSystem> CollisionSystem;

    // Resolves bullet rows (config) and preloads their soft references.
    UPROPERTY()
    TObjectPtr<UBulletConfigSubsystem> ConfigSubsystem;

    bool bEnableDebugDraw = false;
};

