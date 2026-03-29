#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"

#include "ActCharacterMovementTypes.generated.h"

class AActor;
class UAnimMontage;

/**
 * Coordinate space used by translation AddMove.
 *
 * - World: velocity/displacement is already in world space.
 * - Actor: velocity/displacement is authored in capsule space.
 * - Mesh: velocity/displacement is authored in mesh space.
 */
UENUM(BlueprintType)
enum class EActAddMoveSpace : uint8
{
	World,
	Actor,
	Mesh
};

/**
 * Scalar profile applied to authored velocity AddMove.
 */
UENUM(BlueprintType)
enum class EActAddMoveCurveType : uint8
{
	Constant,
	EaseIn,
	EaseOut,
	EaseInOut,
	CustomCurve
};

/**
 * Optional movement-mode gate for authored AddMove.
 * When the current movement mode no longer matches, the AddMove is removed.
 */
UENUM(BlueprintType)
enum class EActAddMoveModeFilter : uint8
{
	Any,
	GroundedOnly,
	AirOnly,
	FallingOnly,
	WalkingOnly,
};

/**
 * Source type for translation AddMove.
 */
UENUM(BlueprintType)
enum class EActAddMoveSourceType : uint8
{
	Velocity,
	MontageRootMotion
};

/**
 * Explicit montage execution mode used by ability tasks.
 *
 * - Procedural: montage is presentation only; native root motion is disabled.
 * - ExtractedRootMotion: montage root motion is extracted into AddMove and executed by CharacterMovement.
 * - NativeRootMotion: UE native anim root motion remains enabled.
 */
UENUM(BlueprintType)
enum class EActMontageExecutionMode : uint8
{
	Procedural,
	ExtractedRootMotion,
	NativeRootMotion,
};

/**
 * Source type for rotation AddMove.
 */
UENUM(BlueprintType)
enum class EActAddMoveRotationSourceType : uint8
{
	Actor,
	Direction
};

/**
 * Actor-based facing policy for rotation AddMove.
 */
UENUM(BlueprintType)
enum class EActAddMoveRotationActorMode : uint8
{
	MatchTargetForward,
	MatchOppositeTargetForward,
	FaceTarget,
	BackToTarget,
};

/**
 * Runtime-authored translation AddMove description consumed by CharacterMovement.
 * Velocity-based moves are explicit displacement overlays, while MontageRootMotion
 * samples an animation range and executes it inside CharacterMovement for prediction.
 */
USTRUCT(BlueprintType)
struct FActAddMoveParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove")
	EActAddMoveSourceType SourceType = EActAddMoveSourceType::Velocity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	EActAddMoveSpace Space = EActAddMoveSpace::World;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	FVector Velocity = FVector::ZeroVector;

	/**
	 * Authored duration in seconds.
	 * - 0: invalid and rejected.
	 * - > 0: finite duration.
	 * - < 0: persistent until explicitly stopped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (ClampMin = "-1"))
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove")
	EActAddMoveModeFilter ModeFilter = EActAddMoveModeFilter::Any;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity && CurveType == EActAddMoveCurveType::CustomCurve"))
	TObjectPtr<UCurveFloat> Curve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	float StartTrackPosition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	float EndTrackPosition = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	bool bApplyRotation = false;

	/**
	 * If true, montage-authored rotation yields to an explicit rotation AddMove when both are active.
	 * This keeps gameplay-authored facing in control during attack startup / hit react alignment.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	bool bRespectAddMoveRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	bool bIgnoreZAccumulate = true;

	/**
	 * Stable identity for prediction, server simulation, and simulated-proxy bootstrap.
	 * Leave INDEX_NONE for local-only authored AddMove.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove")
	int32 SyncId = INDEX_NONE;
};

/**
 * Runtime-authored rotation AddMove consumed by CharacterMovement.
 *
 * Rotation AddMove is the rotation-side twin of translation AddMove:
 * - lock-on locomotion facing
 * - attack startup alignment
 * - hit react "face attacker" alignment
 */
USTRUCT(BlueprintType)
struct FActAddMoveRotationParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation")
	EActAddMoveRotationSourceType SourceType = EActAddMoveRotationSourceType::Actor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation", meta = (EditCondition = "SourceType == EActAddMoveRotationSourceType::Actor"))
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation", meta = (EditCondition = "SourceType == EActAddMoveRotationSourceType::Actor"))
	EActAddMoveRotationActorMode ActorMode = EActAddMoveRotationActorMode::FaceTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation", meta = (EditCondition = "SourceType == EActAddMoveRotationSourceType::Direction"))
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation", meta = (ClampMin = "0"))
	float RotationRate = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation", meta = (ClampMin = "0"))
	float AcceptableYawError = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation")
	bool bClearOnReached = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove Rotation")
	int32 SyncId = INDEX_NONE;

	bool IsValidRequest() const
	{
		switch (SourceType)
		{
		case EActAddMoveRotationSourceType::Actor:
			return ::IsValid(TargetActor);
		case EActAddMoveRotationSourceType::Direction:
			return !Direction.IsNearlyZero();
		default:
			return false;
		}
	}
};

/**
 * State-driven locomotion parameters.
 * This is the parameter-side half of the framework; AddMove handles authored extra motion,
 * while movement state params define how the base locomotion moves/accelerates/turns.
 */
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
