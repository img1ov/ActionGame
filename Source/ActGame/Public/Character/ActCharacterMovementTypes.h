#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"

#include "ActCharacterMovementTypes.generated.h"

class AActor;
class UAnimMontage;

UENUM(BlueprintType)
enum class EActMotionBasisMode : uint8
{
	World,
	ActorStartFrozen,
	ActorLive,
	MeshStartFrozen,
	MeshLive,
};

UENUM(BlueprintType)
enum class EActMotionCurveType : uint8
{
	Constant,
	EaseIn,
	EaseOut,
	EaseInOut,
	CustomCurve,
};

UENUM(BlueprintType)
enum class EActMotionModeFilter : uint8
{
	Any,
	GroundedOnly,
	AirOnly,
	FallingOnly,
	WalkingOnly,
};

UENUM(BlueprintType)
enum class EActMotionSourceType : uint8
{
	Velocity,
	MontageRootMotion,
};

UENUM(BlueprintType)
enum class EActMontageExecutionMode : uint8
{
	Procedural,
	ExtractedRootMotion,
	NativeRootMotion,
};

UENUM(BlueprintType)
enum class EActMotionProvenance : uint8
{
	OwnerPredicted,
	AuthorityExternal,
	ReplicatedExternal,
	LocalRuntime,
};

UENUM(BlueprintType)
enum class EActMotionRotationSourceType : uint8
{
	None,
	Actor,
	Direction,
};

UENUM(BlueprintType)
enum class EActMotionRotationActorMode : uint8
{
	MatchTargetForward,
	MatchOppositeTargetForward,
	FaceTarget,
	BackToTarget,
};

UENUM(BlueprintType)
enum class EActMotionRotationPriority : uint8
{
	Locomotion = 0,
	LockOn = 10,
	Attack = 20,
	HitReact = 30,
};

USTRUCT(BlueprintType)
struct FActMotionRotationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation")
	EActMotionRotationSourceType SourceType = EActMotionRotationSourceType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation", meta = (EditCondition = "SourceType == EActMotionRotationSourceType::Actor"))
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation", meta = (EditCondition = "SourceType == EActMotionRotationSourceType::Actor"))
	EActMotionRotationActorMode ActorMode = EActMotionRotationActorMode::FaceTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation", meta = (EditCondition = "SourceType == EActMotionRotationSourceType::Direction"))
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation", meta = (ClampMin = "0"))
	float AcceptableYawError = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation")
	bool bClearOnReached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation")
	bool bFreezeDirectionAtStart = false;

	/** When true, this rotation also steers active frozen-basis translation motions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation")
	bool bIsAdditive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Rotation")
	EActMotionRotationPriority Priority = EActMotionRotationPriority::Attack;

	bool IsValidRequest() const
	{
		switch (SourceType)
		{
		case EActMotionRotationSourceType::Actor:
			return ::IsValid(TargetActor);
		case EActMotionRotationSourceType::Direction:
			return !Direction.IsNearlyZero();
		default:
			return false;
		}
	}
};

USTRUCT(BlueprintType)
struct FActMotionParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EActMotionSourceType SourceType = EActMotionSourceType::Velocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	EActMotionBasisMode BasisMode = EActMotionBasisMode::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	FVector LaunchVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	bool bOverrideLaunchXY = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	bool bOverrideLaunchZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "-1"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EActMotionModeFilter ModeFilter = EActMotionModeFilter::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity"))
	EActMotionCurveType CurveType = EActMotionCurveType::Constant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::Velocity && CurveType == EActMotionCurveType::CustomCurve"))
	TObjectPtr<UCurveFloat> Curve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	float StartTrackPosition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	float EndTrackPosition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	bool bApplyRootMotionRotation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	bool bRespectHigherPriorityRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (EditCondition = "SourceType == EActMotionSourceType::MontageRootMotion"))
	bool bIgnoreZAccumulate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	FActMotionRotationParams Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EActMotionProvenance Provenance = EActMotionProvenance::OwnerPredicted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	int32 SyncId = INDEX_NONE;

	bool HasTranslation() const
	{
		switch (SourceType)
		{
		case EActMotionSourceType::Velocity:
			return !Velocity.IsNearlyZero();
		case EActMotionSourceType::MontageRootMotion:
			return Montage != nullptr && !FMath::IsNearlyEqual(StartTrackPosition, EndTrackPosition);
		default:
			return false;
		}
	}

	bool HasRotation() const
	{
		return Rotation.IsValidRequest() || (SourceType == EActMotionSourceType::MontageRootMotion && bApplyRootMotionRotation);
	}

	bool HasLaunch() const
	{
		return SourceType == EActMotionSourceType::Velocity && !LaunchVelocity.IsNearlyZero();
	}

	bool IsNetworkSynchronized() const
	{
		return SyncId != INDEX_NONE;
	}
};

USTRUCT()
struct FActReplicatedMotion
{
	GENERATED_BODY()

	UPROPERTY()
	FActMotionParams Params;

	UPROPERTY()
	float ServerStartTimeSeconds = 0.0f;

	UPROPERTY()
	FVector StartLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator FrozenBasisRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FVector FrozenFacingDirection = FVector::ZeroVector;

	bool IsValid() const
	{
		return Params.SyncId != INDEX_NONE;
	}
};

USTRUCT(BlueprintType)
struct FActMovementStateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float MaxWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float MaxAcceleration = 2048.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float BrakingDecelerationWalking = 2048.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float GroundFriction = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	FRotator RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	bool bUseControllerDesiredRotation = false;
};
