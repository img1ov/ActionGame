// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/ActCharacterMovementNetworking.h"
#include "Character/ActCharacterMovementTypes.h"
#include "ActCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccelerationStateChangedSignature, bool, bOldHasAcceleration, bool, bNewHasAcceleration);

class UAnimMontage;
class UCurveFloat;
class USkeletalMeshComponent;
class AActor;

USTRUCT(BlueprintType)
struct FActCharacterGroundInfo
{
	GENERATED_BODY()

	/** Cached data is invalidated per-frame; we store the last frame number we updated on. */
	FActCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{}

	/** Frame counter when this cache was last updated. */
	uint64 LastUpdateFrame;

	/** Floor hit result (walking uses CurrentFloor; otherwise a trace is performed). */
	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	/** Distance from capsule bottom to ground (0 while walking). */
	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

/**
 * Project character movement component.
 *
 * Responsibilities:
 * - Standard UE character movement behavior.
 * - Extra movement overlays ("AddMove"): explicit velocity bursts or animation-derived motion,
 *   executed inside the movement component so it participates in prediction and correction.
 *
 * Non-responsibilities:
 * - Command matching / combo logic (lives in Input + ASC AbilityChain runtime).
 * - Ability cancellation policy / cancel windows (lives in ASC / ability layer).
 */
UCLASS()
class ACTGAME_API UActCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	
	/** Sets defaults and installs our custom network move data container. */
	UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/**
	 * Used by AActCharacter replicated acceleration (COND_SimulatedOnly) to drive simulated proxies.
	 * When bHasReplicatedAcceleration is set, we preserve the replicated acceleration through SimulateMovement.
	 */
	void SetReplicatedAcceleration(const FVector& InAcceleration);
	
	/**
	 * Ground info cache accessor.
	 * Walking uses CurrentFloor; otherwise we trace down to measure ground distance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement")
	const FActCharacterGroundInfo& GetGroundInfo();
	
	/** Convenience accessor used by animation BP to detect acceleration. */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "HasAcceleration"), Category = "Act|CharacterMovement")
	FORCEINLINE bool GetHasAcceleration () const{ return GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER; }

	/**
	 * Adds an explicit velocity-based AddMove entry in world space.
	 *
	 * @param WorldVelocity Extra velocity vector in world space (units/sec).
	 * @param Duration Duration seconds; <0 means infinite until stopped.
	 * @param CurveType Scalar profile applied over time.
	 * @param Curve Optional curve when CurveType is CustomCurve.
	 * @param ExistingHandle If not -1, update an existing entry rather than creating a new one.
	 * @return Handle for stopping/updating the entry, or INDEX_NONE on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMoveWorld(FVector WorldVelocity, float Duration, EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant, UCurveFloat* Curve = nullptr, int32 ExistingHandle = -1);

	/** Same as SetAddMoveWorld, but velocity is in UpdatedComponent (capsule) local space. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMove(FVector LocalVelocity, float Duration, EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant, UCurveFloat* Curve = nullptr, int32 ExistingHandle = -1);

	/** Same as SetAddMove, but basis comes from the mesh component (useful when mesh yaw is offset from capsule). */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMoveWithMesh(USkeletalMeshComponent* Mesh, FVector MeshLocalVelocity, float Duration, EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant, UCurveFloat* Curve = nullptr, int32 ExistingHandle = -1);

	/**
	 * Adds an animation-derived AddMove by sampling root motion on a montage track range.
	 *
	 * The montage itself may be playing for visuals/events, but we disable native montage root motion
	 * and instead execute the extracted displacement through the movement component so it becomes
	 * predictable and network-alignable (via SyncId).
	 */
	int32 SetAddMoveFromMontage(UAnimMontage* Montage, float StartTrackPosition, float EndTrackPosition, float Duration, bool bApplyRotation = false, int32 ExistingHandle = INDEX_NONE, int32 SyncId = INDEX_NONE);

	/** Stops one AddMove entry by handle. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool StopAddMove(int32 Handle);

	/** Stops a mesh-scoped AddMove entry (if one is currently associated with the mesh). */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool StopAddMoveWithMesh(USkeletalMeshComponent* Mesh);

	/** Stops and clears all active AddMove entries. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	void StopAllAddMove();

	/** True if any AddMove entries are active. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|AddMove")
	bool HasActiveAddMove() const;

	/**
	 * Submit or refresh a one-shot rotation warp toward the given target actor.
	 *
	 * Repeated calls overwrite the current request. The movement component will clear the request
	 * automatically once it reaches the acceptable yaw error when bClearOnReached is true, or the
	 * caller can clear it explicitly.
	 */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|RotationWarp")
	void WarpTargetFromRotation(AActor* Target, EActRotationWarpType RotationType, float InRotationRate = 720.0f, float InAcceptableYawError = 2.0f, bool bClearOnReached = true);

	/** Explicitly clears the current rotation warp target. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|RotationWarp")
	void ClearWarpTarget();

	/** C++ accessor used by abilities/notifies to inspect the current warp request, if any. */
	const FActRotationWarpRequest* GetRotationWarpTarget() const;

protected:
	
	/** ~UCharacterMovementComponent Interface */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;
	virtual void PhysicsRotation(float DeltaTime) override;
	/** ~End UCharacterMovementComponent Interface */

private:
	struct FActVelocityAdditionState;

	/** Applies a one-shot rotation warp request before lock-on/default rotation logic. */
	bool TryApplyRotationWarp(float DeltaTime);
	bool TryApplyStrafeRotation(float DeltaTime);
	int32 SetAddMoveInternal(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle);
	FVector ConsumeAddMoveDisplacement(float DeltaSeconds, FQuat& OutRotationDelta);
	float EvaluateAddMoveScale(const FActAddMoveParams& Params, float ElapsedTime, float Duration) const;
	FVector ResolveAddMoveDisplacement(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh, float PreviousElapsedTime, float NewElapsedTime, float DeltaSeconds, bool& bOutHasRotationDelta, FQuat& OutRotationDelta) const;
	FTransform GetAddMoveBasisTransform(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh) const;
	int32 AcquireAddMoveHandle(int32 ExistingHandle, int32 SyncId);
	void ApplyPendingAddMove(float DeltaSeconds);
	void RemoveAddMoveMappings(int32 Handle, const FActVelocityAdditionState* State);
	void CaptureAddMoveSnapshots(TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& OutSnapshots) const;
	void RestoreAddMoveSnapshots(const TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& Snapshots, bool bNetworkSynchronizedOnly);
	uint32 GetAddMoveStateRevision() const { return AddMoveStateRevision; }

	struct FActVelocityAdditionState
	{
		int32 Handle = INDEX_NONE;
		FActAddMoveParams Params;
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		float ElapsedTime = 0.0f;
	};

	friend struct FActCharacterNetworkMoveData;
	friend class FActSavedMove_Character;

	
public:
	
	UPROPERTY(BlueprintAssignable, Category="Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;
	
protected:
	
	FActCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;
	
	bool bHasAcceleration = false;

	/** Active AddMove entries by handle. */
	TMap<int32, FActVelocityAdditionState> VelocityAdditionMap;
	/** Mesh -> handle mapping for mesh-scoped entries. */
	TMap<TObjectPtr<USkeletalMeshComponent>, int32> VelocityAdditionMapByMesh;
	/** Next local handle (only meaningful on this machine). */
	int32 NextAddMoveHandle = 1;
	/** Re-entrancy guard while we apply displacement into UpdatedComponent. */
	bool bApplyingAddMove = false;
	/** Pending displacement computed in ConsumeAddMoveDisplacement and applied in OnMovementUpdated. */
	FVector PendingAddMoveDisplacement = FVector::ZeroVector;
	/** Pending rotation delta (only used when bApplyRotation on montage-derived entries). */
	FQuat PendingAddMoveRotationDelta = FQuat::Identity;

	/**
	 * When AddMove is active, we temporarily relax server correction to reduce jitter under bad network.
	 * This is restored when the last AddMove ends.
	 */
	bool bAddMoveNetworkCorrectionOverrideActive = false;
	bool bCachedIgnoreClientMovementErrorChecksAndCorrection = false;
	bool bCachedServerAcceptClientAuthoritativePosition = false;

	/** Increments every time AddMove state meaningfully changes; used to gate SavedMove combining. */
	uint32 AddMoveStateRevision = 0;

	/** SyncId -> handle mapping for network-stable entries (section refresh uses this to "update" not "add"). */
	TMap<int32, int32> VelocityAdditionMapBySyncId;

	/** Current one-shot rotation warp request; cleared once aligned or when the target becomes invalid. */
	FActRotationWarpRequest RotationWarpTarget;

	/** Custom network move data payload container (New/Pending/Old) used by CMC RPC packing. */
	mutable FActCharacterNetworkMoveDataContainer NetworkMoveDataContainer;

	/** Enables/disables network correction relax mode depending on whether AddMove is active. */
	void UpdateAddMoveNetworkCorrectionMode();
};
