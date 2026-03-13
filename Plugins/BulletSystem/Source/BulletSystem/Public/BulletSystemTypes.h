#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
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
    DestroyBullet,
    DelayDestroyBullet,
    SceneInteract,
    UpdateEffect,
    UpdateLiveTime,
    UpdateAttackerFrozen
};

UENUM(BlueprintType)
enum class EBulletDestroyReason : uint8
{
    LifeTimeExpired,
    Hit,
    SkillEnd,
    ParentDestroyed,
    Logic,
    External
};

UENUM(BlueprintType)
enum class EBulletSyncType : uint8
{
    None,
    Server,
    Multicast
};

UENUM(BlueprintType)
enum class EBulletCollisionResponse : uint8
{
    Destroy,
    Bounce,
    Pierce
};

USTRUCT(BlueprintType)
struct FBulletDataBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    float LifeTime = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletShapeType Shape = EBulletShapeType::Sphere;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float SphereRadius = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    FVector BoxExtent = FVector(10.0f, 10.0f, 10.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float CapsuleRadius = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "0"))
    float CapsuleHalfHeight = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    bool bDestroyOnHit = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base", meta = (ClampMin = "1"))
    int32 MaxHitCount = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    EBulletCollisionResponse CollisionResponse = EBulletCollisionResponse::Destroy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
    FGameplayTagContainer Tags;
};

USTRUCT(BlueprintType)
struct FBulletDataLogic
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    FGameplayTagContainer LogicTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Logic")
    bool bEnableHitEffect = true;
};

USTRUCT(BlueprintType)
struct FBulletDataAimed
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aimed")
    bool bUseAim = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aimed", meta = (ClampMin = "0"))
    float AimAngleTolerance = 5.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataMove
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    EBulletMoveType MoveType = EBulletMoveType::Straight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float Speed = 1200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    FVector InitialVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    float Gravity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bUseOwnerForward = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bUseSpawnTransform = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bHoming = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0"))
    float HomingAcceleration = 8000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0"))
    float OrbitRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move", meta = (ClampMin = "0"))
    float OrbitAngularSpeed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move")
    bool bAttachToOwner = false;
};

USTRUCT(BlueprintType)
struct FBulletDataRender
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSoftObjectPtr<UNiagaraSystem> NiagaraSystem;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    TSubclassOf<ABulletActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    FVector MeshScale = FVector(1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    bool bAttachToOwner = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Render")
    bool bAutoActivate = true;
};

USTRUCT(BlueprintType)
struct FBulletDataTimeScale
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Time")
    float TimeDilation = 1.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataExecution
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Execution")
    TArray<TSoftObjectPtr<UBulletLogicData>> LogicDataList;
};

USTRUCT(BlueprintType)
struct FBulletDataScale
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
    FVector Scale = FVector(1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scale")
    bool bUseOwnerScale = false;
};

USTRUCT(BlueprintType)
struct FBulletDataSummon
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    TSubclassOf<AActor> SummonClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    FTransform SummonOffset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Summon")
    bool bAttachToOwner = false;
};

USTRUCT(BlueprintType)
struct FBulletDataChild
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    FName ChildRowName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child", meta = (ClampMin = "0"))
    int32 Count = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child", meta = (ClampMin = "0"))
    float SpreadAngle = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    bool bInheritOwner = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    bool bInheritTarget = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    bool bSpawnOnDestroy = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Child")
    bool bSpawnOnHit = false;
};

USTRUCT(BlueprintType)
struct FBulletDataObstacle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle")
    bool bEnableObstacle = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Obstacle", meta = (ClampMin = "0"))
    float ObstacleTraceRadius = 0.0f;
};

USTRUCT(BlueprintType)
struct FBulletDataInteract
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bEnableInteract = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact")
    bool bAffectEnvironment = false;
};

USTRUCT(BlueprintType)
struct FBulletDataMain : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FName RowName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataBase Base;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataLogic Logic;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataAimed Aimed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataMove Move;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataRender Render;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataTimeScale TimeScale;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataExecution Execution;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataScale Scale;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataSummon Summon;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataChild Children;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataObstacle Obstacle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Main")
    FBulletDataInteract Interact;

    bool CheckSimpleBullet() const
    {
        const bool bHasLogic = Execution.LogicDataList.Num() > 0;
        const bool bHasChildren = !Children.ChildRowName.IsNone() && Children.Count > 0;
        const bool bHasSummon = Summon.SummonClass != nullptr;
        const bool bHasInteract = Interact.bEnableInteract || Obstacle.bEnableObstacle;
        return !bHasLogic && !bHasChildren && !bHasSummon && !bHasInteract;
    }
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    int32 SkillId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    int32 ContextId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    EBulletSyncType SyncType = EBulletSyncType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FVector SizeOverride = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    FVector SpawnOffset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    int32 ParentBulletId = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Init")
    float OverrideLifeTime = -1.0f;
};

USTRUCT(BlueprintType)
struct FBulletActionInfo
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    EBulletActionType Type = EBulletActionType::InitBullet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    float Delay = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    EBulletDestroyReason DestroyReason = EBulletDestroyReason::External;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    bool bSpawnChildren = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    FName ChildRowName;
};
