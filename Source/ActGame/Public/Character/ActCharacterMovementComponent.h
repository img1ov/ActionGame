#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Character/ActCharacterMovementNetworking.h"
#include "Character/ActCharacterMovementTypes.h"
#include "ActCharacterMovementComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAccelerationStateChangedSignature, bool, bOldHasAcceleration, bool, bNewHasAcceleration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroundStateChangedSignature, bool, bOldIsOnGround, bool, bNewIsOnGround);

class UAnimMontage;
class USkeletalMeshComponent;
class UGameplayAbility;

USTRUCT(BlueprintType)
struct FActCharacterGroundInfo
{
	GENERATED_BODY()

	FActCharacterGroundInfo()
		: LastUpdateFrame(0)
		, GroundDistance(0.0f)
	{
	}

	uint64 LastUpdateFrame;

	UPROPERTY(BlueprintReadOnly)
	FHitResult GroundHitResult;

	UPROPERTY(BlueprintReadOnly)
	float GroundDistance;
};

UCLASS()
class ACTGAME_API UActCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer);

	static UActCharacterMovementComponent* ResolveActMovementComponent(const AActor* AvatarActor);
	static bool ShouldUseMotionNetworkSync(const UGameplayAbility* Ability, EActMotionProvenance Provenance);
	static int32 BuildStableMotionSyncId(FName InstanceName, const UGameplayAbility* Ability, const TCHAR* FallbackSeed);

	void SetReplicatedAcceleration(const FVector& InAcceleration);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement")
	const FActCharacterGroundInfo& GetGroundInfo();

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DisplayName = "HasAcceleration"), Category = "Act|CharacterMovement")
	bool GetHasAcceleration() const
	{
		return GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER;
	}

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ApplyMovementStateParams(const FActMovementStateParams& Params);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ResetMovementStateParams();

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	int32 PushMovementStateParams(const FActMovementStateParams& Params, int32 ExistingHandle = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	bool PopMovementStateParams(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|State")
	void ClearMovementStateParamsStack();

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	int32 GenerateMotionSyncId();

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	int32 ApplyMotion(const FActMotionParams& Params, USkeletalMeshComponent* Mesh = nullptr, int32 ExistingHandle = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	int32 ApplyRotationMotion(
		const FActMotionRotationParams& RotationParams,
		float Duration = 0.25f,
		EActMotionModeFilter ModeFilter = EActMotionModeFilter::Any,
		EActMotionProvenance Provenance = EActMotionProvenance::OwnerPredicted,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	int32 AddMoveRotationToTarget(
		AActor* TargetActor,
		EActMotionRotationActorMode ActorMode = EActMotionRotationActorMode::FaceTarget,
		float Duration = 0.25f,
		bool bIsAdditive = false,
		EActMotionRotationPriority Priority = EActMotionRotationPriority::Attack,
		EActMotionProvenance Provenance = EActMotionProvenance::OwnerPredicted);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	int32 AddMoveRotationToDirection(
		FVector Direction,
		float Duration = 0.25f,
		bool bIsAdditive = false,
		EActMotionRotationPriority Priority = EActMotionRotationPriority::Attack,
		EActMotionProvenance Provenance = EActMotionProvenance::OwnerPredicted);

	int32 ApplyRootMotionMotion(
		UAnimMontage* Montage,
		float StartTrackPosition,
		float EndTrackPosition,
		float Duration,
		EActMotionBasisMode BasisMode = EActMotionBasisMode::MeshStartFrozen,
		bool bApplyRootMotionRotation = false,
		bool bRespectHigherPriorityRotation = true,
		bool bIgnoreZAccumulate = true,
		FActMotionRotationParams Rotation = FActMotionRotationParams{},
		EActMotionProvenance Provenance = EActMotionProvenance::OwnerPredicted,
		int32 ExistingHandle = -1,
		int32 SyncId = -1);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	bool StopMotion(int32 Handle);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	bool StopMotionBySyncId(int32 SyncId);

	UFUNCTION(BlueprintCallable, Category = "Act|CharacterMovement|Motion")
	void StopAllMotion();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|Motion")
	bool HasActiveMotion() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Act|CharacterMovement|Motion")
	bool HasMotionBySyncId(int32 SyncId) const;

	void ExtendExternalLaunchCorrectionGrace(float GraceDuration);
	bool HasActiveReplicatedExternalMotion() const;
	bool ShouldSuppressSharedReplication(const FVector& ServerLocation) const;
	void ApplySuppressedSharedReplication(const FVector& ServerLocation, const FVector& ServerVelocity);

	void SyncReplicatedMotions(const TArray<FActReplicatedMotion>& ReplicatedMotions);
	void CapturePredictedMotionSnapshots(TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>>& OutSnapshots) const;
	void RestorePredictedMotionSnapshots(const TArray<FActPredictedMotionSnapshot, TInlineAllocator<4>>& Snapshots, bool bNetworkSynchronizedOnly);
	void BuildReplicatedMotionEntries(TArray<FActReplicatedMotion>& OutEntries) const;

protected:
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual bool CanAttemptJump() const override;
	virtual void PhysicsRotation(float DeltaTime) override;

private:
	struct FActMotionState;

	struct FActMovementStateStackEntry
	{
		int32 Handle = INDEX_NONE;
		FActMovementStateParams Params;
	};

	struct FActResolvedMotionRotation
	{
		bool bHasRootMotionRotation = false;
		FQuat RootMotionDelta = FQuat::Identity;
	};

	friend struct FActCharacterNetworkMoveData;
	friend class FActSavedMove_Character;

	int32 ApplyMotionInternal(FActMotionParams Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle, float InitialElapsedTime = 0.0f);
	bool StopMotionInternal(int32 Handle);
	void ApplyPendingMotion(float DeltaSeconds);
	FVector ConsumeMotionDisplacement(float DeltaSeconds, FActResolvedMotionRotation& OutRotation);
	float EvaluateMotionScale(const FActMotionParams& Params, float ElapsedTime, float Duration) const;
	bool DoesMotionPassModeFilter(const FActMotionParams& Params) const;
	void ApplyMotionLaunch(FActMotionState& State, float ElapsedTime);
	FVector ResolveMotionDisplacement(const FActMotionState& State, float PreviousElapsedTime, float NewElapsedTime, float DeltaSeconds, FQuat& OutRootMotionRotationDelta) const;
	FVector ResolveMotionLaunchVelocity(const FActMotionState& State, float ElapsedTime) const;
	FVector ResolveMotionLaunchDisplacement(const FActMotionState& State, float ElapsedTime) const;
	FTransform ResolveMotionBasisTransform(const FActMotionState& State) const;
	FVector ResolveMotionFacingDirection(const FActMotionState& State) const;
	void ApplyReplicatedMotionCatchUp(FActMotionState& State, float PreviousElapsedTime, float NewElapsedTime);
	void ApplyAdditiveRotationToActiveMotionBases(int32 RotationHandle, const FQuat& RotationDelta);
	void ApplyAbsoluteRotationToActiveMotionBases(int32 RotationHandle, const FRotator& NewBasisRotation);
	void CaptureRuntimeBasis(FActMotionState& State);
	int32 AcquireMotionHandle(int32 ExistingHandle, int32 SyncId);
	void UpdateLockOnRotationTask();
	void RefreshActiveRotationTask();
	bool TickActiveRotationTask(float DeltaSeconds, FRotator& InOutFinalRotation);
	bool CanStateDriveRotationTask(const FActMotionState& State) const;
	void RemoveMotionEntries(const TArray<int32>& HandlesToRemove);
	void RemoveMotionMappings(int32 Handle, const FActMotionState* State);
	void RefreshNetworkCorrectionMode();
	void RefreshMovementStateParamsFromStack();
	float GetReplicatedServerWorldTimeSeconds() const;
	void RequestReplicatedMotionRefresh() const;
	bool HasBlockingMotionRotation() const;
	uint32 GetMotionStateRevision() const { return MotionStateRevision; }

	struct FActMotionState
	{
		int32 Handle = INDEX_NONE;
		FActMotionParams Params;
		TWeakObjectPtr<USkeletalMeshComponent> BasisMesh;
		float ElapsedTime = 0.0f;
		float ServerStartTimeSeconds = 0.0f;
		FVector StartLocation = FVector::ZeroVector;
		FRotator FrozenBasisRotation = FRotator::ZeroRotator;
		FVector FrozenFacingDirection = FVector::ZeroVector;
		bool bHasFrozenBasisRotation = false;
		bool bHasFrozenFacingDirection = false;
		bool bRotationCompleted = false;
		bool bRotationPaused = false;
		bool bRootMotionRotationSuppressed = false;
		bool bLaunchApplied = false;
		bool bHasStartLocation = false;
		float RotationElapsedTime = 0.0f;
	};

public:
	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnAccelerationStateChangedSignature OnAccelerationStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Act|CharacterMovement")
	FOnGroundStateChangedSignature OnGroundStateChanged;

protected:
	FActCharacterGroundInfo CachedGroundInfo;

	UPROPERTY(Transient)
	bool bHasReplicatedAcceleration = false;

	bool bHasAcceleration = false;
	bool bIsOnGround = false;

	TMap<int32, FActMotionState> MotionStateMap;
	TMap<int32, int32> MotionStateMapBySyncId;
	TSet<int32> ConfirmedOwnerPredictedMotionSyncIds;
	int32 NextMotionHandle = 1;
	int32 NextGeneratedMotionSyncId = 1;
	int32 NextMovementStateHandle = 1;
	int32 ActiveRotationHandle = INDEX_NONE;
	int32 LockOnRotationHandle = INDEX_NONE;

	bool bApplyingMotion = false;
	FVector PendingMotionDisplacement = FVector::ZeroVector;
	FQuat PendingMotionRotation = FQuat::Identity;

	bool bMotionNetworkCorrectionOverrideActive = false;
	bool bCachedIgnoreClientMovementErrorChecksAndCorrection = false;
	bool bCachedServerAcceptClientAuthoritativePosition = false;
	float ExternalLaunchCorrectionGraceEndTime = 0.0f;
	uint32 MotionStateRevision = 0;

	FActMovementStateParams DefaultMovementStateParams;
	TArray<FActMovementStateStackEntry> MovementStateStack;

	mutable FActCharacterNetworkMoveDataContainer NetworkMoveDataContainer;
};
