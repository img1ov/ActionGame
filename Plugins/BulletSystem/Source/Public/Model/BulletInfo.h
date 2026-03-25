#pragma once

// BulletSystem: BulletInfo.h
// Runtime bullet state storage.

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "BulletSystemTypes.h"
#include "BulletInfo.generated.h"

class UBulletConfig;
class UBulletEntity;
class ABulletActor;
class UNiagaraComponent;
class UCurveVector;

USTRUCT(BlueprintType)
struct FBulletMoveInfo
{
    GENERATED_BODY()

    // Current world location of the bullet (cm).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Location = FVector::ZeroVector;

    // Last tick location (used for sweep/trace start).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector LastLocation = FVector::ZeroVector;

    // Current velocity (cm/s).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Velocity = FVector::ZeroVector;

    // Current acceleration (cm/s^2). Used by some movement modes.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector Acceleration = FVector::ZeroVector;

    // Current rotation used by render/aim/box/capsule overlap orientation.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FRotator Rotation = FRotator::ZeroRotator;

    // Whether MoveInfo has been initialized for this bullet instance.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bInitialized = false;

    // If true, the move system will keep the bullet bound to its owner (shield/attached bullets).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bAttachToOwner = false;

    // Cached relative transform used when following/attaching to the owner (computed when we enter attached state).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FTransform AttachedRelativeTransform = FTransform::Identity;

    // Tracks whether we were considered attached in the previous tick (used to detect attachment transitions).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bAttachedLastTick = false;

    // Orbit state (degrees).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float OrbitAngle = 0.0f;

    // Orbit center in world space (cm).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector OrbitCenter = FVector::ZeroVector;

    // Whether OrbitCenter has been initialized.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bOrbitCenterInitialized = false;

    // Optional curve used by custom curve-driven movement.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    TObjectPtr<UCurveVector> CustomMoveCurve = nullptr;

    // Total duration for curve-driven movement (seconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float CustomMoveDuration = 0.0f;

    // Elapsed time for curve-driven movement (seconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float CustomMoveElapsed = 0.0f;

    // Cached origin for curve-driven movement to avoid cumulative drift.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector CustomMoveStartLocation = FVector::ZeroVector;

    // Fixed-duration movement support.
    // Start location for fixed-duration movement.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector FixedStartLocation = FVector::ZeroVector;

    // Target location for fixed-duration movement.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector FixedTargetLocation = FVector::ZeroVector;

    // Total duration of fixed-duration movement (seconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float FixedDuration = 0.0f;

    // Elapsed time for fixed-duration movement (seconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float FixedElapsed = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletCollisionInfo
{
    GENERATED_BODY()

    // Whether collision queries are enabled for this bullet instance.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    bool bCollisionEnabled = true;

    // Total accepted hit count (respects filtering + HitInterval + collision response).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    int32 HitCount = 0;

    // World time (seconds) of the last accepted hit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    float LastHitTime = 0.0f;

    // Monotonic batch id for accepted hits. Increments whenever we accept a new hit batch (manual processing call
    // or auto-collision tick). This is used to reliably group multi-target hits even when multiple batches happen
    // within the same frame (WorldTime can be identical).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    int32 LastHitBatchId = 0;

    // True if any hit was accepted in the current collision tick.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision")
    bool bHitThisFrame = false;

    // Per-actor last hit time to support interval damage.
    TMap<TWeakObjectPtr<AActor>, float> HitActors;

    // Actors accepted in the most recent hit batch (LastHitBatchId).
    // This is a transient snapshot used by "apply to all hit actors" logic controllers.
    TArray<TWeakObjectPtr<AActor>> LastBatchHitActors;

    // Snapshot of actors currently overlapping this bullet shape (overlap mode) or hit along the trace (ray mode).
    // This is used for manual-hit triggering and for "damage over time" style hit processing.
    TSet<TWeakObjectPtr<AActor>> OverlapActors;
};

USTRUCT(BlueprintType)
struct FBulletEffectInfo
{
    GENERATED_BODY()

    // Spawned Niagara component (if using Niagara rendering). Owned by the bullet actor.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
    TObjectPtr<UNiagaraComponent> NiagaraComponent = nullptr;
};

USTRUCT(BlueprintType)
struct FBulletChildInfo
{
    GENERATED_BODY()

    // Total number of children spawned so far for this bullet (debug/limit support).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    int32 SpawnedCount = 0;
};

USTRUCT(BlueprintType)
struct FBulletRayInfo
{
    GENERATED_BODY()

    // Last ray trace start (only meaningful when Shape == Ray).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray")
    FVector TraceStart = FVector::ZeroVector;

    // Last ray trace end (only meaningful when Shape == Ray).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ray")
    FVector TraceEnd = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FBulletInfo
{
    GENERATED_BODY()

    // Runtime instance id assigned by the model. Unique within the current world/subsystem.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    int32 InstanceId = INDEX_NONE;

    // Spawn parameters captured at creation time (owner/target/transform/ability ids, payload).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletInitParams InitParams;

    // Resolved config for this instance (copied from data table/config asset).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletDataMain Config;

    // Config asset used to resolve Config for this instance (optional).
    // Needed so child bullets can resolve rows from the same override config asset instead of falling back to the global config subsystem.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TObjectPtr<UBulletConfig> SourceConfigAsset = nullptr;

    // Lightweight runtime entity wrapper (logic/budget/actor binding).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TObjectPtr<UBulletEntity> Entity = nullptr;

    // Optional render actor (pooled). Collision is handled by scene queries, not components.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TObjectPtr<ABulletActor> Actor = nullptr;

    // Movement runtime state.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletMoveInfo MoveInfo;

    // Collision runtime state/caches.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletCollisionInfo CollisionInfo;

    // Effect runtime state (Niagara, etc).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletEffectInfo EffectInfo;

    // Child bullet runtime state.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletChildInfo ChildInfo;

    // Ray trace debug/state.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FBulletRayInfo RayInfo;

    // True once InitBullet has run and the instance is considered active.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bIsInit = false;

    // Marked for destruction; controller will flush and return resources at end of tick.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bNeedDestroy = false;

    // Fast-path marker for simple bullets without extra logic/summon/child/interact.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bIsSimple = false;

    // World time when the bullet instance was created.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float SpawnWorldTime = 0.0f;

    // Parent instance id for child bullets (INDEX_NONE if no parent).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    int32 ParentInstanceId = INDEX_NONE;

    // World time when destroy action was triggered (used for parent-child lifecycle binding).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float DestroyWorldTime = -1.0f;

    // Time since spawn (seconds). Driven by UpdateLiveTime action.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float LiveTime = 0.0f;

    // Pending delayed destroy time (seconds). Used by DelayDestroyBullet action.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float PendingDestroyDelay = 0.0f;

    // Per-instance time dilation (multiplies movement tick delta).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float TimeScale = 1.0f;

    // Per-instance size multiplier (defaults to (1,1,1)). Currently used by render scaling.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FVector Size = FVector::OneVector;

    // Frozen state (project-specific). Driven by logic/controller.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    bool bFrozen = false;

    // World time until frozen expires (seconds).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    float FrozenUntilTime = 0.0f;

    // Instance tags (merged from config + runtime additions).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    FGameplayTagContainer Tags;

    // Action queue currently being executed by the action runner.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TArray<FBulletActionInfo> ActionInfoList;

    // When actions are enqueued while the runner is already iterating ActionInfoList, they are redirected here
    // to avoid modifying the active list during iteration. The runner will swap this into ActionInfoList
    // once the current batch is complete.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Info")
    TArray<FBulletActionInfo> NextActionInfoList;
};
