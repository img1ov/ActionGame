#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"

#include "ActCharacterMovementTypes.generated.h"

class UAnimMontage;

/**
 * Coordinate space used by AddMove.
 *
 * - World: velocity/displacement is already in world space.
 * - Actor: velocity/displacement is in the UpdatedComponent (capsule) space.
 * - Mesh: velocity/displacement is in the mesh component space (useful when mesh has yaw offset).
 */
UENUM(BlueprintType)
enum class EActAddMoveSpace : uint8
{
	World,
	Actor,
	Mesh
};

/**
 * Scalar profile for explicit velocity-based AddMove.
 *
 * The curve is evaluated with Alpha = Elapsed / Duration and scales the authored velocity.
 * MontageRootMotion moves ignore this and instead sample the animation root motion range.
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
 * Source of an AddMove entry.
 *
 * - Velocity: explicit extra movement (sprint burst, knockback impulse, skill dash, etc).
 * - MontageRootMotion: derived from animation root motion between a start/end track position,
 *   executed by the movement component so it participates in prediction and correction.
 */
UENUM(BlueprintType)
enum class EActAddMoveSourceType : uint8
{
	Velocity,
	MontageRootMotion
};

/**
 * Runtime-authored displacement description consumed by the movement framework.
 * Velocity-based moves are explicit extra movement, while MontageRootMotion
 * samples a section range from animation and turns it into predicted movement.
 */
USTRUCT(BlueprintType)
struct FActAddMoveParams
{
	GENERATED_BODY()

	/** Selects which fields are active below (velocity vs montage root motion sampling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove")
	EActAddMoveSourceType SourceType = EActAddMoveSourceType::Velocity;

	/** Coordinate space for Velocity moves (ignored for MontageRootMotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	EActAddMoveSpace Space = EActAddMoveSpace::World;

	/** Authored velocity vector (units/sec) for Velocity moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	FVector Velocity = FVector::ZeroVector;

	/**
	 * Authored duration (seconds).
	 * - 0: invalid, will be rejected by the movement component.
	 * - > 0: finite duration.
	 * - < 0: infinite duration (persistent until explicitly stopped).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (ClampMin = "-1"))
	float Duration = 0.0f;

	/** Profile type used to scale Velocity over time (ignored for MontageRootMotion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity"))
	EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant;

	/** Optional scalar curve for Velocity moves. Alpha is Elapsed/Duration in [0,1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::Velocity && CurveType == EActAddMoveCurveType::CustomCurve"))
	TObjectPtr<UCurveFloat> Curve = nullptr;

	/** Montage whose root motion track will be sampled (MontageRootMotion only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** Track start time (seconds) for root motion sampling (MontageRootMotion only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	float StartTrackPosition = 0.0f;

	/** Track end time (seconds) for root motion sampling (MontageRootMotion only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	float EndTrackPosition = 0.0f;

	/** Whether to apply extracted rotation delta (MontageRootMotion only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AddMove", meta = (EditCondition = "SourceType == EActAddMoveSourceType::MontageRootMotion"))
	bool bApplyRotation = false;

	/**
	 * Runtime sync identifier used to align predicted AddMove state between
	 * client replay, movement RPCs, and server simulation.
	 * Leave as INDEX_NONE for local-only moves.
	 */
	UPROPERTY(Transient)
	int32 SyncId = INDEX_NONE;
};
