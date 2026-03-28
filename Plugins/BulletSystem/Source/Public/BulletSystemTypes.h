#pragma once

// BulletSystem: BulletSystemTypes.h
// Core data types and config structs.

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/ObjectPtr.h"
#include "BulletSystemTypes.generated.h"

class UBulletLogicData;
class UNiagaraSystem;
class UStaticMesh;
class AActor;
class ABulletActor;

UENUM(BlueprintType)
enum class EBulletShapeType : uint8
{
    Sphere,
    Box,
    Capsule,
    Ray
};

UENUM(BlueprintType)
enum class EBulletConfigProfile : uint8
{
    HitBox,
    Projectile,
    Custom
};

UENUM(BlueprintType)
enum class EBulletMoveType : uint8
{
    Straight,
    Parabola,
    Orbit,
    FollowTarget,
    FixedDuration,
    Attached,
    Custom
};

UENUM(BlueprintType)
enum class EBulletLogicTrigger : uint8
{
    OnBegin,
    OnHit,
    OnDestroy,
    OnRebound,
    OnSupport,
    ReplaceMove,
    OnHitBullet,
    Tick
};

UENUM(BlueprintType)
enum class EBulletActionType : uint8
{
    InitBullet,
    InitHit,
    InitMove,
    InitCollision,
    InitRender,
    TimeScale,
    AfterInit,
    Child,
    SummonBullet,
    SummonEntity,
    DelayDestroyBullet,
    SceneInteract,
    UpdateEffect,
    UpdateLiveTime,
    UpdateAttackerFrozen
};

// Destroy reasons were intentionally removed. "Destroy" is always an explicit request that takes effect at end of tick,
// and lifetime expiry is handled by the built-in lifetime tick.

UENUM(BlueprintType)
enum class EBulletCollisionResponse : uint8
{
    Destroy = 0,
    Bounce = 1,
    Pierce = 2,
    Support = 3
};

UENUM(BlueprintType)
enum class EBulletCollisionMode : uint8
{
    Sweep,
    Overlap
};

UENUM(BlueprintType)
enum class EBulletHitTrigger : uint8
{
    EachFrame,
    Manual
};

UENUM(BlueprintType)
enum class EImpactDirection : uint8
{
    Up,
    Down,
    Left,
    Right
};

USTRUCT(BlueprintType)
struct FHitReactImpulse
{
    GENERATED_BODY()

    // GameplayEvent tag that drives GA_HitReact (ability should be triggered by this event tag).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    FGameplayTag HitReactTag;
    
    // Direction for hit reaction visuals
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    EImpactDirection VisualImpactDirection;

    // Impulse direction/amount in attacker-relative space:
    // +Y = away from attacker, +X = attacker-right, +Z = world up.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    FVector ImpulseVector = FVector::ZeroVector;

    // Primary strength used for scaling / montage selection / convenience (also mirrored into EventData.EventMagnitude).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    float Strength = 0.0f;

    // Optional strength-over-time curve used by the target hit-react ability when converting this impulse
    // into procedural movement. This scales Strength over normalized time.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitReact")
    TObjectPtr<UCurveFloat> StrengthCurve = nullptr;
    
};

USTRUCT(BlueprintType)
struct FBulletPayload
{
    GENERATED_BODY()

    // Optional per-shot payload carried by the bullet instance.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (PinHiddenByDefault))
    TMap<FName, float> SetByCallerNameMagnitudes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (PinHiddenByDefault))
    TMap<FGameplayTag, float> SetByCallerTagMagnitudes;

    // Optional hit-react payload injected at spawn time (typically via AnimNotify/GA).
    // If UBulletLogicData_ApplyGameplayEffect.bApplyHitReact is enabled, this will be forwarded to the target ASC via
    // GameplayEventData.TargetData (see FHitReactImpulseTargetData).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (PinHiddenByDefault))
    FHitReactImpulse HitReactImpulse;
};

USTRUCT(BlueprintType)
struct FBulletDataBase
{
    GENERATED_BODY()

    /** Lifetime in seconds. When exceeded the bullet is destroyed (unless overridden by logic). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    float LifeTime = 5.0f;

    /** Collision shape used by the collision system (sweep/overlap/trace). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletShapeType Shape = EBulletShapeType::Sphere;

    /** Sphere radius in Unreal units (cm). Only used when Shape == Sphere. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0", EditCondition = "Shape == EBulletShapeType::Sphere"))
    float SphereRadius = 10.0f;

    /** Box half-extents in Unreal units (cm). Only used when Shape == Box. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (EditCondition = "Shape == EBulletShapeType::Box"))
    FVector BoxExtent = FVector(10.0f, 10.0f, 10.0f);

    /** Capsule radius in Unreal units (cm). Only used when Shape == Capsule. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0", EditCondition = "Shape == EBulletShapeType::Capsule"))
    float CapsuleRadius = 10.0f;

    /** Capsule half-height in Unreal units (cm). Only used when Shape == Capsule. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0", EditCondition = "Shape == EBulletShapeType::Capsule"))
    float CapsuleHalfHeight = 20.0f;

    /** If true, the bullet will be destroyed once a hit is registered and collision response allows it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    bool bDestroyOnHit = true;

    /** Maximum number of hits before the bullet is forced to stop hitting (used with pierce/overlap). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "1"))
    int32 MaxHitCount = 1;

    /** High-level response policy when a hit is accepted (destroy/bounce/pierce/support). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletCollisionResponse CollisionResponse = EBulletCollisionResponse::Destroy;

    /** How the collision system queries the world (sweep vs overlap). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletCollisionMode CollisionMode = EBulletCollisionMode::Sweep;

    /** Whether the collision system triggers hits automatically or stores overlaps for manual triggering. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletHitTrigger HitTrigger = EBulletHitTrigger::EachFrame;

    /** Initial collision enabled state on spawn (can be toggled later via controller). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    bool bCollisionEnabledOnSpawn = true;

    /** Budget tick interval for high-frequency systems (0 = every frame). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float BudgetTickInterval = 0.0f;

    /** Delay before collision becomes active after spawn (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float CollisionStartDelay = 0.0f;

    /** Minimum interval between hits on the same actor (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float HitInterval = 0.0f;

    /** Collision channel used for sweep/overlap/trace queries. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

    /** Gameplay tags carried by this bullet instance (used by logic/filtering). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct FBulletDataLogic
{
    GENERATED_BODY()

    /** Designer-defined tags for logic controllers (routing, filtering, special rules). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    FGameplayTagContainer LogicTags;

    /** Tags on the owner that indicate "frozen" state (config-driven, no hard-coded tags). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    FGameplayTagContainer OwnerFrozenTags;
};

USTRUCT(BlueprintType)
struct FBulletDataAimed
{
    GENERATED_BODY()

    /** If true, the bullet expects aim constraints to be applied/validated by gameplay code. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aimed")
    bool bUseAim = false;

    /** Allowed deviation (degrees) when aim validation is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aimed", meta = (ClampMin = "0", EditCondition = "bUseAim"))
    float AimAngleTolerance = 5.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataMove
{
    GENERATED_BODY()

    /** Movement model applied by the move system. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    EBulletMoveType MoveType = EBulletMoveType::Straight;

    /** Base speed in cm/s for movement modes that use speed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float Speed = 1200.0f;

    /** Optional initial velocity override (cm/s). If zero, velocity is derived from spawn/aim settings. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector InitialVelocity = FVector::ZeroVector;

    /** Gravity acceleration (cm/s^2). Only used when MoveType == Parabola. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (EditCondition = "MoveType == EBulletMoveType::Parabola"))
    float Gravity = 0.0f;

    /** Spawn position offset (cm). Interpreted in spawn transform space. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector SpawnLocationOffset = FVector::ZeroVector;

    /** Rotation offset applied after aim/spawn-forward resolution (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FRotator SpawnRotationOffset = FRotator::ZeroRotator;

    /** If true, steer toward TargetActor every tick (straight/fixed-duration only). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (EditCondition = "MoveType == EBulletMoveType::Straight || MoveType == EBulletMoveType::FixedDuration"))
    bool bHoming = false;

    /** Homing acceleration (cm/s^2). Higher values turn faster. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", EditCondition = "bHoming && (MoveType == EBulletMoveType::Straight || MoveType == EBulletMoveType::FixedDuration)"))
    float HomingAcceleration = 8000.0f;

    /** For FixedDuration movement, time to reach the target (seconds). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", EditCondition = "MoveType == EBulletMoveType::FixedDuration"))
    float FixedDuration = 0.0f;

    /** Orbit radius (cm). Only used when MoveType == Orbit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", EditCondition = "MoveType == EBulletMoveType::Orbit"))
    float OrbitRadius = 0.0f;

    /** Orbit angular speed (deg/s). Only used when MoveType == Orbit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0", EditCondition = "MoveType == EBulletMoveType::Orbit"))
    float OrbitAngularSpeed = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataRender
{
    GENERATED_BODY()

    /** Niagara system to spawn for this bullet (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    /** Static mesh to display (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    /** Actor class to use for rendering (optional). Falls back to BulletSystemSettings.DefaultBulletActorClass. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSubclassOf<ABulletActor> ActorClass;

    /** Mesh scale applied on the render actor (if StaticMesh is used). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

    /** If true, auto-activate Niagara/mesh components when the render actor is acquired. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    bool bAutoActivate = true;
};

USTRUCT(BlueprintType)
struct FBulletDataTimeScale
{
    GENERATED_BODY()

    /** Per-bullet time dilation applied by the move system (scaled delta = delta * TimeDilation). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale")
    float TimeDilation = 1.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataExecution
{
    GENERATED_BODY()

    /** Logic data assets that spawn logic controllers for this bullet (soft refs to allow streaming). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution")
    TArray<TSoftObjectPtr<UBulletLogicData>> LogicDataList;
};

USTRUCT(BlueprintType)
struct FBulletDataScale
{
    GENERATED_BODY()

    /** Default scale multiplier for this bullet instance. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
    FVector Scale = FVector(1.0f, 1.0f, 1.0f);

    /** If true, multiply Scale by the owner actor scale at spawn. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
    bool bUseOwnerScale = false;
};

USTRUCT(BlueprintType)
struct FBulletDataSummon
{
    GENERATED_BODY()

    /** Optional actor class to summon/spawn as part of bullet lifecycle (e.g. decal, explosion actor). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    TSubclassOf<AActor> SummonClass;

    /** Offset transform applied relative to bullet spawn/impact depending on summon implementation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    FTransform SummonOffset;

    /** If true, attach the summoned actor to the owner. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    bool bAttachToOwner = false;
};

USTRUCT(BlueprintType)
struct FBulletDataChild
{
    GENERATED_BODY()

    /** BulletId to spawn as a child (looked up in config). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    FName ChildBulletId;

    /** Number of child bullets to spawn when triggered. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children", meta = (ClampMin = "0"))
    int32 Count = 1;

    /** Location offset applied to the spawned child bullet's spawn transform (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    FVector SpawnLocationOffset = FVector::ZeroVector;

    /** If true, SpawnLocationOffset is interpreted in child spawn rotation space; otherwise in world space. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bSpawnLocationOffsetInSpawnSpace = true;

    /** Rotation offset applied to the spawned child bullet's spawn transform (degrees). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    FRotator SpawnRotationOffset = FRotator::ZeroRotator;

    /** If true, child inherits owner from parent bullet. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bInheritOwner = true;

    /** If true, child inherits target from parent bullet. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bInheritTarget = true;

    /** If true, child inherits per-shot payload from parent bullet (InitParams.Payload). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bInheritPayload = true;

    /** If true, spawn these children when the parent bullet is destroyed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bSpawnOnDestroy = false;

    /** If true, spawn these children when the parent bullet hits something. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
    bool bSpawnOnHit = false;
};

USTRUCT(BlueprintType)
struct FBulletDataObstacle
{
    GENERATED_BODY()

    /** If true, run an extra static-only obstacle sweep before normal collision processing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
    bool bEnableObstacle = false;

    /** Obstacle sweep radius (cm). If 0, falls back to Base.SphereRadius. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle", meta = (ClampMin = "0", EditCondition = "bEnableObstacle"))
    float ObstacleTraceRadius = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataInteract
{
    GENERATED_BODY()

    /** If true, attempt to interact with hit actors via BulletInteractInterface (project-specific). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bEnableInteract = false;

    /** If true, allow interactions to affect the environment (e.g. breakables) when enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (EditCondition = "bEnableInteract"))
    bool bAffectEnvironment = false;
};

USTRUCT(BlueprintType)
struct FBulletDataMain : public FTableRowBase
{
    GENERATED_BODY()

    /** Primary identifier for this row. If used in a DataTable, RowName can also act as id depending on loader. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FName BulletId;

    /** Convenience profile: auto-fills some defaults in editor (HitBox/Projectile/Custom). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    EBulletConfigProfile ConfigProfile = EBulletConfigProfile::Projectile;

    /** Tracks the last profile applied for editor auto-fill (editor-only helper; not used at runtime). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Main", AdvancedDisplay, meta = (HideInDetailPanel))
    EBulletConfigProfile AppliedProfile = EBulletConfigProfile::Custom;

    /** Core behavior: lifetime, collision shape/mode, channels, hit rules. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ShowOnlyInnerProperties))
    FBulletDataBase Base;

    /** Logic behavior: tags and special rules (e.g. frozen tags, hit effects). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic", meta = (ShowOnlyInnerProperties))
    FBulletDataLogic Logic;

    /** Aim constraints (optional). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aimed", meta = (ShowOnlyInnerProperties))
    FBulletDataAimed Aimed;

    /** Movement parameters for the move system. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ShowOnlyInnerProperties))
    FBulletDataMove Move;

    /** Render parameters (niagara/mesh/actor class). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render", meta = (ShowOnlyInnerProperties))
    FBulletDataRender Render;

    /** Per-bullet time scale (affects movement tick). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale", meta = (ShowOnlyInnerProperties))
    FBulletDataTimeScale TimeScale;

    /** Execution logic controllers (soft-referenced assets). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution", meta = (ShowOnlyInnerProperties))
    FBulletDataExecution Execution;

    /** Per-bullet scale settings. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale", meta = (ShowOnlyInnerProperties))
    FBulletDataScale Scale;

    /** Optional actor summon settings (spawn additional actor on events). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon", meta = (ShowOnlyInnerProperties))
    FBulletDataSummon Summon;

    /** Child bullet spawners triggered on hit/destroy (see FBulletDataChild flags). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children", meta = (TitleProperty = "ChildBulletId"))
    TArray<FBulletDataChild> Children;

    /** Extra static obstacle checks. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle", meta = (ShowOnlyInnerProperties))
    FBulletDataObstacle Obstacle;

    /** Optional interaction hooks (project-specific). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact", meta = (ShowOnlyInnerProperties))
    FBulletDataInteract Interact;

    /** Returns true if this bullet row has no logic/children/summon/interact features and can use the fast path. */
    bool CheckSimpleBullet() const
    {
        const bool bHasLogic = Execution.LogicDataList.Num() > 0;
        bool bHasChildren = false;
        for (const FBulletDataChild& Child : Children)
        {
            if (!Child.ChildBulletId.IsNone() && Child.Count > 0)
            {
                bHasChildren = true;
                break;
            }
        }
        const bool bHasSummon = Summon.SummonClass != nullptr;
        const bool bHasInteract = Interact.bEnableInteract || Obstacle.bEnableObstacle;
        return !bHasLogic && !bHasChildren && !bHasSummon && !bHasInteract;
    }

    /** Applies editor-time defaults based on ConfigProfile. Safe to call repeatedly. */
    void ApplyConfigProfileDefaults(bool bForce = false)
    {
        if (!bForce && AppliedProfile == ConfigProfile)
        {
            return;
        }

        AppliedProfile = ConfigProfile;

        switch (ConfigProfile)
        {
        case EBulletConfigProfile::HitBox:
        {
            Base = FBulletDataBase();
            Move = FBulletDataMove();
            Render = FBulletDataRender();

            Base.LifeTime = 0.2f;
            Base.Shape = EBulletShapeType::Box;
            Base.BoxExtent = FVector(50.0f, 50.0f, 50.0f);
            Base.bDestroyOnHit = false;
            Base.MaxHitCount = 999;
            Base.CollisionResponse = EBulletCollisionResponse::Pierce;
            Base.CollisionMode = EBulletCollisionMode::Overlap;
            Base.HitTrigger = EBulletHitTrigger::Manual;
            Base.CollisionChannel = ECC_Pawn;

            Move.MoveType = EBulletMoveType::Straight;
            Move.Speed = 0.0f;
            break;
        }
        case EBulletConfigProfile::Projectile:
        {
            Base = FBulletDataBase();
            Move = FBulletDataMove();
            Render = FBulletDataRender();

            Base.Shape = EBulletShapeType::Sphere;
            Base.SphereRadius = 10.0f;
            Base.bDestroyOnHit = true;
            Base.MaxHitCount = 1;
            Base.CollisionResponse = EBulletCollisionResponse::Destroy;
            Base.CollisionMode = EBulletCollisionMode::Sweep;
            Base.HitTrigger = EBulletHitTrigger::EachFrame;
            Base.CollisionChannel = ECC_WorldDynamic;

            Move.MoveType = EBulletMoveType::Straight;
            Move.Speed = 1200.0f;
            break;
        }
        case EBulletConfigProfile::Custom:
        default:
            break;
        }
    }

#if WITH_EDITOR
    virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutProblems) override
    {
        ApplyConfigProfileDefaults();
    }

    virtual void OnDataTableChanged(const UDataTable* InDataTable, const FName InRowName) override
    {
        ApplyConfigProfileDefaults();
    }
#endif
};

USTRUCT(BlueprintType)
struct FBulletInitParams
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    TObjectPtr<AActor> Owner = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FTransform SpawnTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FVector TargetLocation = FVector::ZeroVector;

    // Per-instance payload (typically filled by GA/Gameplay code at spawn time).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FBulletPayload Payload;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    int32 ParentInstanceId = INDEX_NONE;

    // Optional per-instance key used for runtime lookup (e.g. AnimNotify spawn -> later process/destroy by name).
    // This is a local runtime label and is not used by BulletSystem core simulation.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FName InstanceKey = NAME_None;
};

USTRUCT(BlueprintType)
struct FBulletActionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    EBulletActionType Type = EBulletActionType::InitBullet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    float Delay = 0.0f;

    // Destroy metadata and spawn-children toggles were removed:
    // - destroy is always an explicit request that takes effect at end of tick
    // - lifetime expiry uses the same destroy path
    // - child spawning is governed by bullet config (bSpawnOnDestroy / bSpawnOnHit)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    FName ChildBulletId;

    // Optional override for SummonBullet actions (-1 = use matching child config Count).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 SpawnCount = -1;

    // Optional override for SummonBullet actions (-1 = use logic/data default; child spawners typically set 0).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    float SpreadAngle = -1.0f;

    // Optional spawn offsets for SummonBullet actions (used by child spawners / logic).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    FVector SpawnLocationOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    bool bSpawnLocationOffsetInSpawnSpace = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    FRotator SpawnRotationOffset = FRotator::ZeroRotator;

    // Optional inherit override for SummonBullet actions (-1 = use config or default true).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 InheritOwner = -1;

    // Optional inherit override for SummonBullet actions (-1 = use config or default true).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 InheritTarget = -1;

    // Optional inherit override for SummonBullet actions (-1 = use config or default true).
    // Controls whether child inherits InitParams.Payload.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 InheritPayload = -1;
};
