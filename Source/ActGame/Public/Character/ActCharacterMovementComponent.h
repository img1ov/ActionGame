// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/ActCharacterMovementNetworking.h"
#include "Character/ActCharacterMovementTypes.h"
#include "ActCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccelerationStateChangedSignature, bool, bOldHasAcceleration, bool, bNewHasAcceleration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroundStateChangedSignature, bool, bOldIsOnGround, bool, bNewIsOnGround);

class AActor;
class UAnimMontage;
class UCurveFloat;
class USkeletalMeshComponent;

USTRUCT(BlueprintType)
struct FActCharacterGroundInfo
{
	GENERATED_BODY()

	FActCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{
	}

	/** Frame guard so one movement tick only refreshes ground info once. */
	uint64 LastUpdateFrame;

	/** Cached floor hit used by Blueprint-side hit / land / ground-distance queries. */
	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	/** Capsule-bottom to ground distance in world units. */
	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

/**
 * CharacterMovement implementation aligned to the project's AddMove framework.
 *
 * Framework split:
 * - Base locomotion: input + movement state params (speed/acceleration/turning).
 * - Translation AddMove: explicit extra movement such as launch, knockback, dash.
 * - Rotation AddMove: explicit extra facing such as lock-on strafe, attack face target, hit react face attacker.
 * - RootMotion: optional third path. When extracted, it is converted into AddMove and still executed here.
 */
UCLASS()
class ACTGAME_API UActCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	/** Replicated simulated-proxy acceleration setter used during movement replay. */
	void SetReplicatedAcceleration(const FVector& InAcceleration);

	/** Returns one-frame cached ground info for Blueprint-side landing / floor queries. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement")
	const FActCharacterGroundInfo& GetGroundInfo();

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "HasAcceleration"), Category = "Act|CharacterMovement")
	bool GetHasAcceleration() const
	{
		return GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
	}

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ApplyMovementStateParams(const FActMovementStateParams& Params);

	/** Restores locomotion parameters back to the defaults captured from CharacterMovement. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ResetMovementStateParams();

	/** Pushes one locomotion state override; the stack top always owns current locomotion params. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	int32 PushMovementStateParams(const FActMovementStateParams& Params, int32 ExistingHandle = -1);

	/** Removes one locomotion state override previously returned by PushMovementStateParams. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	bool PopMovementStateParams(int32 Handle);

	/** Clears every locomotion override and falls back to the default locomotion params. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ClearMovementStateParamsStack();

	/**
	 * Allocates one runtime SyncId for server-driven or Blueprint-authored AddMove chains.
	 * Predicted ability tasks should prefer deterministic task-side SyncIds instead.
	 */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 GenerateAddMoveSyncId();

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMoveWorld(
		FVector WorldVelocity,
		float Duration,
		EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant,
		UCurveFloat* Curve = nullptr,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMove(
		FVector LocalVelocity,
		float Duration,
		EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant,
		UCurveFloat* Curve = nullptr,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMoveWithMesh(
		USkeletalMeshComponent* Mesh,
		FVector MeshLocalVelocity,
		float Duration,
		EActAddMoveCurveType CurveType = EActAddMoveCurveType::Constant,
		UCurveFloat* Curve = nullptr,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 ApplyAddMoveParams(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh = nullptr, int32 ExistingHandle = -1);

	/** Converts one authored root-motion range into AddMove so CMC owns execution/prediction instead of AnimRM. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	int32 SetAddMoveFromRootMotionRange(
		UAnimMontage* Montage,
		float StartTrackPosition,
		float EndTrackPosition,
		float Duration,
		bool bApplyRotation = false,
		bool bRespectAddMoveRotation = true,
		bool bIgnoreZAccumulate = true,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool StopAddMove(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool StopAddMoveWithMesh(USkeletalMeshComponent* Mesh);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool StopAddMoveBySyncId(int32 SyncId);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool PauseAddMove(int32 Handle);

	/** Decrements the pause lock count for one AddMove; it resumes once the count reaches zero. */
	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool ResumeAddMove(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool PauseAddMoveBySyncId(int32 SyncId);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	bool ResumeAddMoveBySyncId(int32 SyncId);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove")
	void StopAllAddMove();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|AddMove")
	bool HasActiveAddMove() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|AddMove")
	bool HasAddMoveBySyncId(int32 SyncId) const;

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove Rotation")
	void SetAddMoveRotationToActor(
		AActor* Target,
		EActAddMoveRotationActorMode ActorMode,
		float InRotationRate = 720.0f,
		float AcceptableYawError = 2.0f,
		bool bClearOnReached = true,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove Rotation")
	void SetAddMoveRotationToDirection(
		FVector WorldDirection,
		float InRotationRate = 720.0f,
		float AcceptableYawError = 2.0f,
		bool bClearOnReached = true,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|AddMove Rotation")
	void ClearAddMoveRotation();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|AddMove Rotation")
	bool HasActiveAddMoveRotation() const;

	/** Active explicit facing request currently overriding default locomotion yaw. */
	const FActAddMoveRotationParams* GetAddMoveRotationParams() const;

	/** Simulated-proxy bootstrap helpers driven from AActCharacter multicast RPCs. */
	void ApplyReplicatedAddMove(const FActAddMoveParams& Params);
	void StopReplicatedAddMove(int32 SyncId);
	void PauseReplicatedAddMove(int32 SyncId);
	void ResumeReplicatedAddMove(int32 SyncId);
	void ApplyReplicatedAddMoveRotation(const FActAddMoveRotationParams& Params);
	void ClearReplicatedAddMoveRotation();

protected:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	struct FActAddMoveState;

	/** Applies explicit rotation AddMove before any lock-on/default locomotion rotation. */
	bool TryApplyAddMoveRotation(float DeltaTime);
	/** Lock-on strafe facing fallback used only when no explicit rotation AddMove owns yaw. */
	bool TryApplyStrafeRotation(float DeltaTime);
	int32 SetAddMoveInternal(FActAddMoveParams Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle, bool bBroadcastToSimulatedProxies);
	bool StopAddMoveInternal(int32 Handle, bool bBroadcastToSimulatedProxies);
	bool PauseAddMoveInternal(int32 Handle);
	bool ResumeAddMoveInternal(int32 Handle);
	/** Consumes one frame of translation/rotation contributed by every active AddMove entry. */
	FVector ConsumeAddMoveDisplacement(float DeltaSeconds, FQuat& OutRotationDelta);
	float EvaluateAddMoveScale(const FActAddMoveParams& Params, float ElapsedTime, float Duration) const;
	bool DoesAddMovePassModeFilter(const FActAddMoveParams& Params) const;
	FVector ResolveAddMoveDisplacement(
		const FActAddMoveParams& Params,
		USkeletalMeshComponent* Mesh,
		float PreviousElapsedTime,
		float NewElapsedTime,
		float DeltaSeconds,
		bool& bOutHasRotationDelta,
		FQuat& OutRotationDelta) const;
	FTransform GetAddMoveBasisTransform(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh) const;
	int32 AcquireAddMoveHandle(int32 ExistingHandle, int32 SyncId);
	/** Executes the frame-accumulated AddMove delta after base CharacterMovement has updated. */
	void ApplyPendingAddMove(float DeltaSeconds);
	void RemoveAddMoveMappings(int32 Handle, const FActAddMoveState* State);
	void RemoveAddMoves(const TArray<int32>& Handles);
	void CaptureAddMoveSnapshots(TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& OutSnapshots) const;
	void RestoreAddMoveSnapshots(const TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& Snapshots, bool bNetworkSynchronizedOnly);
	uint32 GetAddMoveStateRevision() const { return AddMoveStateRevision; }
	void UpdateAddMoveNetworkCorrectionMode();
	void SetAddMoveRotationInternal(const FActAddMoveRotationParams& Params, bool bBroadcastToSimulatedProxies);
	void ClearAddMoveRotationInternal(bool bBroadcastToSimulatedProxies);
	/** Re-applies the top-most locomotion state after any stack mutation. */
	void RefreshMovementStateParamsFromStack();

	struct FActAddMoveState
	{
		/** Local-only runtime handle; never leaves this machine. */
		int32 Handle = INDEX_NONE;
		/** Authored AddMove definition. */
		FActAddMoveParams Params;
		/** Optional mesh basis for mesh-space AddMove. */
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		/** Time already consumed inside this AddMove. */
		float ElapsedTime = 0.0f;
		/** Reference-counted pause lock used by gameplay and replication. */
		int32 PauseLockCount = 0;
	};

	struct FActMovementStateStackEntry
	{
		/** Local stack handle returned to Blueprint/gameplay code. */
		int32 Handle = INDEX_NONE;
		/** Authored locomotion parameter override. */
		FActMovementStateParams Params;
	};

	friend struct FActCharacterNetworkMoveData;
	friend class FActSavedMove_Character;

public:
	/** Broadcast when 2D acceleration presence toggles. */
	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;

	/** Broadcast when grounded state toggles; intended for Blueprint-side hit/launch state transitions. */
	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnGroundStateChangedSignature OnGroundStateChanged;

protected:
	/** Cached ground trace/floor result reused during one movement tick. */
	FActCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;

	bool bHasAcceleration = false;
	bool bIsOnGround = false;

	/** Active translation AddMove states keyed by local handle. */
	TMap<int32, FActAddMoveState> AddMoveStateMap;
	/** Mesh-scoped AddMove lookup. */
	TMap<TObjectPtr<USkeletalMeshComponent>, int32> AddMoveStateMapByMesh;
	/** SyncId-scoped AddMove lookup used by prediction and simulated-proxy bootstrap. */
	TMap<int32, int32> AddMoveStateMapBySyncId;
	int32 NextAddMoveHandle = 1;
	int32 NextGeneratedAddMoveSyncId = 1;
	int32 NextMovementStateHandle = 1;

	/** Guards against AddMove recursively re-entering movement while it is being applied. */
	bool bApplyingAddMove = false;
	/** One-frame pending translation delta computed after AddMove consumption. */
	FVector PendingAddMoveDisplacement = FVector::ZeroVector;
	/** One-frame pending rotation delta produced by montage-derived AddMove rotation. */
	FQuat PendingAddMoveRotationDelta = FQuat::Identity;

	/** While AddMove is active, owner/server correction is relaxed to reduce jitter under poor network. */
	bool bAddMoveNetworkCorrectionOverrideActive = false;
	bool bCachedIgnoreClientMovementErrorChecksAndCorrection = false;
	bool bCachedServerAcceptClientAuthoritativePosition = false;
	/** SavedMove combine gate; any meaningful AddMove state change bumps this revision. */
	uint32 AddMoveStateRevision = 0;

	/** Explicit facing request currently owned by gameplay. */
	FActAddMoveRotationParams AddMoveRotationParams;
	/** Locomotion defaults captured from base CharacterMovement setup. */
	FActMovementStateParams DefaultMovementStateParams;
	/** Top-most entry owns locomotion params; used by sprint/strafe/attack state changes. */
	TArray<FActMovementStateStackEntry> MovementStateStack;

	/** Custom move payload container used to pack AddMove state through CMC RPCs. */
	mutable FActCharacterNetworkMoveDataContainer NetworkMoveDataContainer;
};
