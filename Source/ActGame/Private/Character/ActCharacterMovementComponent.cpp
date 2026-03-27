// Fill out your copyright notice in the Description page of Project Settings.
#include "Character/ActCharacterMovementComponent.h"

#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"
#include "Character/ActBattleComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace ActCharacter
{
	static float GroundTraceDistance = 100000.0f;
};

UActCharacterMovementComponent::UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
	bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;

	// Install our custom move data container so AddMove snapshots are serialized through CMC RPCs.
	// This is the core that allows the server to simulate the same predicted "extra movement"
	// the client applied (reducing correction jitter under bad network conditions).
	SetNetworkMoveDataContainer(NetworkMoveDataContainer);
}

int32 UActCharacterMovementComponent::SetAddMoveWorld(FVector WorldVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::World;
	Params.Velocity = WorldVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	return SetAddMoveInternal(Params, nullptr, ExistingHandle);
}

int32 UActCharacterMovementComponent::SetAddMove(FVector LocalVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::Actor;
	Params.Velocity = LocalVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	return SetAddMoveInternal(Params, nullptr, ExistingHandle);
}

int32 UActCharacterMovementComponent::SetAddMoveWithMesh(USkeletalMeshComponent* Mesh, FVector MeshLocalVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::Mesh;
	Params.Velocity = MeshLocalVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	return SetAddMoveInternal(Params, Mesh, ExistingHandle);
}

int32 UActCharacterMovementComponent::SetAddMoveFromMontage(UAnimMontage* Montage, float StartTrackPosition, float EndTrackPosition, float Duration, bool bApplyRotation, int32 ExistingHandle, int32 SyncId)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::MontageRootMotion;
	Params.Space = EActAddMoveSpace::Actor;
	Params.Duration = Duration;
	Params.Montage = Montage;
	Params.StartTrackPosition = StartTrackPosition;
	Params.EndTrackPosition = EndTrackPosition;
	Params.bApplyRotation = bApplyRotation;
	Params.SyncId = SyncId;

	// Important: montage-derived movement is still executed as "AddMove" (i.e. displacement overlay),
	// not as native montage root motion. The montage is free to play for visuals/events while the
	// authoritative movement is simulated here for prediction + networking.
	return SetAddMoveInternal(Params, nullptr, ExistingHandle);
}

bool UActCharacterMovementComponent::StopAddMove(int32 Handle)
{
	if (Handle == INDEX_NONE)
	{
		return false;
	}

	RemoveAddMoveMappings(Handle, VelocityAdditionMap.Find(Handle));

	const bool bRemoved = VelocityAdditionMap.Remove(Handle) > 0;
	if (bRemoved)
	{
		++AddMoveStateRevision;
	}
	UpdateAddMoveNetworkCorrectionMode();
	return bRemoved;
}

bool UActCharacterMovementComponent::StopAddMoveWithMesh(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return false;
	}

	if (const int32* Handle = VelocityAdditionMapByMesh.Find(Mesh))
	{
		const int32 HandleValue = *Handle;
		RemoveAddMoveMappings(HandleValue, VelocityAdditionMap.Find(HandleValue));
		const bool bRemoved = VelocityAdditionMap.Remove(HandleValue) > 0;
		if (bRemoved)
		{
			++AddMoveStateRevision;
		}
		UpdateAddMoveNetworkCorrectionMode();
		return bRemoved;
	}

	return false;
}

void UActCharacterMovementComponent::StopAllAddMove()
{
	VelocityAdditionMap.Empty();
	VelocityAdditionMapByMesh.Empty();
	VelocityAdditionMapBySyncId.Empty();
	PendingAddMoveDisplacement = FVector::ZeroVector;
	PendingAddMoveRotationDelta = FQuat::Identity;
	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();
}

bool UActCharacterMovementComponent::HasActiveAddMove() const
{
	return VelocityAdditionMap.Num() > 0;
}

void UActCharacterMovementComponent::PushCancelWindow(UObject* SourceObject)
{
	if (!SourceObject)
	{
		return;
	}

	CancelWindowSources.Add(SourceObject);
}

void UActCharacterMovementComponent::PopCancelWindow(UObject* SourceObject)
{
	if (!SourceObject)
	{
		return;
	}

	CancelWindowSources.Remove(SourceObject);
}

void UActCharacterMovementComponent::ClearCancelWindows()
{
	CancelWindowSources.Reset();
}

bool UActCharacterMovementComponent::HasCancelWindow() const
{
	for (const TWeakObjectPtr<UObject>& SourceObject : CancelWindowSources)
	{
		if (SourceObject.IsValid())
		{
			return true;
		}
	}

	return false;
}

FNetworkPredictionData_Client* UActCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr);

	if (!ClientPredictionData)
	{
		UActCharacterMovementComponent* MutableThis = const_cast<UActCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FActNetworkPredictionData_Client_Character(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = NetworkMaxSmoothUpdateDistance;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = NetworkNoSmoothUpdateDistance;
	}

	return ClientPredictionData;
}

void UActCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	if (const FActCharacterNetworkMoveData* ActMoveData = static_cast<const FActCharacterNetworkMoveData*>(GetCurrentNetworkMoveData()))
	{
		RestoreAddMoveSnapshots(ActMoveData->AddMoveSnapshots, true);
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UActCharacterMovementComponent::SimulateMovement(float DeltaTime)
{
	if (bHasReplicatedAcceleration)
	{
		const FVector OriginalAcceleration = Acceleration;
		Super::SimulateMovement(DeltaTime);
		Acceleration = OriginalAcceleration;
	}
	else
	{
		Super::SimulateMovement(DeltaTime);
	}
}

bool UActCharacterMovementComponent::CanAttemptJump() const
{
	// Same as UCharacterMovementComponent's implementation but without the crouch check
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling()); // Falling included for double-jump and non-zero jump hold time, but validated by character.
}

const FActCharacterGroundInfo& UActCharacterMovementComponent::GetGroundInfo()
{
	if (!CharacterOwner || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = (UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn);
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, (TraceStart.Z - ActCharacter::GroundTraceDistance - CapsuleHalfHeight));

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = ActCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max((HitResult.Distance - CapsuleHalfHeight), 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

void UActCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation,
	const FVector& OldVelocity)
{
	UCharacterMovementComponent::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	const bool bOldHasAcceleration = bHasAcceleration;
	const float Accel2DSizeSq = GetHasAcceleration();
	bHasAcceleration = Accel2DSizeSq > KINDA_SMALL_NUMBER;

	if (bHasAcceleration != bOldHasAcceleration)
	{
		OnAccelerationStateChanged.Broadcast(bOldHasAcceleration, bHasAcceleration);
	}

	// ApplyPendingAddMove executes the displacement overlay after the base movement update.
	//
	// Why here:
	// - We want AddMove to "stack" on top of regular character movement, root motion sources, etc.
	// - We keep it in one place so prediction/replay uses the exact same order on both sides.
	ApplyPendingAddMove(DeltaSeconds);
}

void UActCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

void UActCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(CharacterOwner);
	const bool bStrafeMoveActive = TagInterface && TagInterface->HasMatchingGameplayTag(Event_Movement_StrafeMove);
	if (bStrafeMoveActive)
	{
		// In strafe-move state we never auto-orient to movement; only rotate when lock-on can drive it.
		TryApplyStrafeRotation(DeltaTime);
		return;
	}

	Super::PhysicsRotation(DeltaTime);
}

bool UActCharacterMovementComponent::TryApplyStrafeRotation(float DeltaTime)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	// Only apply lock-on facing while the character is actually moving.
	const bool bHasMoveInput = GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER || Velocity.SizeSquared2D() > KINDA_SMALL_NUMBER;
	if (!bHasMoveInput)
	{
		return false;
	}

	UActBattleComponent* BattleComp = CharacterOwner->FindComponentByClass<UActBattleComponent>();
	AActor* LockOnTarget = BattleComp ? BattleComp->GetLockOnTarget() : nullptr;
	if (!LockOnTarget)
	{
		// Strafe is active but no valid target; keep current facing.
		return false;
	}

	const FVector ToTarget = (LockOnTarget->GetActorLocation() - CharacterOwner->GetActorLocation());
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}

	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	FRotator DesiredRotation = ToTarget.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	const float YawRate = RotationRate.Yaw;
	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, DeltaTime, YawRate);
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);
	return true;
}

int32 UActCharacterMovementComponent::SetAddMoveInternal(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle)
{
	if (Params.Duration == 0.0f)
	{
		return INDEX_NONE;
	}

	const int32 Handle = AcquireAddMoveHandle(ExistingHandle, Params.SyncId);
	FActVelocityAdditionState& State = VelocityAdditionMap.FindOrAdd(Handle);
	RemoveAddMoveMappings(Handle, &State);
	State.Handle = Handle;
	State.Params = Params;
	State.Mesh = Mesh;
	State.ElapsedTime = 0.0f;

	if (Mesh)
	{
		VelocityAdditionMapByMesh.FindOrAdd(Mesh) = Handle;
	}
	if (Params.SyncId != INDEX_NONE)
	{
		VelocityAdditionMapBySyncId.FindOrAdd(Params.SyncId) = Handle;
	}

	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();
	return Handle;
}

FVector UActCharacterMovementComponent::ConsumeAddMoveDisplacement(float DeltaSeconds, FQuat& OutRotationDelta)
{
	FVector TotalDisplacement = FVector::ZeroVector;
	OutRotationDelta = FQuat::Identity;

	TArray<int32> HandlesToRemove;
	for (TPair<int32, FActVelocityAdditionState>& Pair : VelocityAdditionMap)
	{
		const int32 Handle = Pair.Key;
		FActVelocityAdditionState& State = Pair.Value;

		if (State.Params.SourceType == EActAddMoveSourceType::Velocity && State.Params.Space == EActAddMoveSpace::Mesh && !State.Mesh.IsValid())
		{
			HandlesToRemove.Add(Handle);
			continue;
		}

		const float PreviousElapsedTime = State.ElapsedTime;
		const bool bInfiniteDuration = State.Params.Duration < 0.0f;
		const float RemainingDuration = bInfiniteDuration ? DeltaSeconds : FMath::Max(State.Params.Duration - PreviousElapsedTime, 0.0f);
		if (!bInfiniteDuration && RemainingDuration <= UE_SMALL_NUMBER)
		{
			HandlesToRemove.Add(Handle);
			continue;
		}

		const float EffectiveDeltaTime = bInfiniteDuration ? DeltaSeconds : FMath::Min(DeltaSeconds, RemainingDuration);
		const float NewElapsedTime = PreviousElapsedTime + EffectiveDeltaTime;

		bool bHasRotationDelta = false;
		FQuat RotationDelta = FQuat::Identity;
		const FVector AddDisplacement = ResolveAddMoveDisplacement(State.Params, State.Mesh.Get(), PreviousElapsedTime, NewElapsedTime, DeltaSeconds, bHasRotationDelta, RotationDelta);
		if (AddDisplacement.ContainsNaN())
		{
			HandlesToRemove.Add(Handle);
			continue;
		}

		TotalDisplacement += AddDisplacement;
		if (bHasRotationDelta)
		{
			OutRotationDelta = RotationDelta * OutRotationDelta;
		}

		State.ElapsedTime = NewElapsedTime;
		if (!bInfiniteDuration && State.ElapsedTime >= State.Params.Duration - UE_SMALL_NUMBER)
		{
			HandlesToRemove.Add(Handle);
		}
	}

	for (const int32 Handle : HandlesToRemove)
	{
		StopAddMove(Handle);
	}

	return TotalDisplacement;
}

float UActCharacterMovementComponent::EvaluateAddMoveScale(const FActAddMoveParams& Params, float ElapsedTime, float Duration) const
{
	if (Duration <= UE_SMALL_NUMBER)
	{
		return 1.0f;
	}

	const float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
	switch (Params.CurveType)
	{
	case EActAddMoveCurveType::Constant:
		return 1.0f;
	case EActAddMoveCurveType::EaseIn:
		return Alpha * Alpha;
	case EActAddMoveCurveType::EaseOut:
		return 1.0f - FMath::Square(1.0f - Alpha);
	case EActAddMoveCurveType::EaseInOut:
		return FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f);
	case EActAddMoveCurveType::CustomCurve:
		return Params.Curve ? Params.Curve->GetFloatValue(Alpha) : 1.0f;
	default:
		return 1.0f;
	}
}

FVector UActCharacterMovementComponent::ResolveAddMoveDisplacement(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh, float PreviousElapsedTime, float NewElapsedTime, float DeltaSeconds, bool& bOutHasRotationDelta, FQuat& OutRotationDelta) const
{
	bOutHasRotationDelta = false;
	OutRotationDelta = FQuat::Identity;

	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	if (Params.SourceType == EActAddMoveSourceType::MontageRootMotion)
	{
		if (!Params.Montage || Params.Duration <= UE_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		const float PreviousAlpha = FMath::Clamp(PreviousElapsedTime / Params.Duration, 0.0f, 1.0f);
		const float CurrentAlpha = FMath::Clamp(NewElapsedTime / Params.Duration, 0.0f, 1.0f);
		const float StartTrackPosition = FMath::Lerp(Params.StartTrackPosition, Params.EndTrackPosition, PreviousAlpha);
		const float EndTrackPosition = FMath::Lerp(Params.StartTrackPosition, Params.EndTrackPosition, CurrentAlpha);
		FTransform DeltaTransform = Params.Montage->ExtractRootMotionFromTrackRange(StartTrackPosition, EndTrackPosition, FAnimExtractContext());
		if (!Params.bApplyRotation)
		{
			DeltaTransform.SetRotation(FQuat::Identity);
		}

		if (Params.bApplyRotation)
		{
			bOutHasRotationDelta = !DeltaTransform.GetRotation().Equals(FQuat::Identity);
			OutRotationDelta = DeltaTransform.GetRotation();
		}

		// Convert animation root motion translation into world space.
		//
		// We use mesh yaw as basis when possible because many characters keep mesh rotated relative
		// to the capsule (e.g. -90 yaw) for asset forward alignment. Using capsule yaw would make
		// the extracted motion travel sideways for those setups.
		FRotator BasisRotation = FRotator::ZeroRotator;
		if (CharacterOwner && CharacterOwner->GetMesh())
		{
			BasisRotation.Yaw = CharacterOwner->GetMesh()->GetComponentRotation().Yaw;
		}
		else if (UpdatedComponent)
		{
			BasisRotation.Yaw = UpdatedComponent->GetComponentRotation().Yaw;
		}

		return BasisRotation.Quaternion().RotateVector(DeltaTransform.GetTranslation());
	}

	const FTransform Basis = GetAddMoveBasisTransform(Params, Mesh);
	const float Scale = Params.Duration > 0.0f ? EvaluateAddMoveScale(Params, NewElapsedTime, Params.Duration) : 1.0f;
	return Basis.TransformVectorNoScale(Params.Velocity * Scale) * DeltaSeconds;
}

FTransform UActCharacterMovementComponent::GetAddMoveBasisTransform(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh) const
{
	switch (Params.Space)
	{
	case EActAddMoveSpace::Actor:
		return UpdatedComponent ? UpdatedComponent->GetComponentTransform() : FTransform::Identity;
	case EActAddMoveSpace::Mesh:
		return Mesh ? Mesh->GetComponentTransform() : (UpdatedComponent ? UpdatedComponent->GetComponentTransform() : FTransform::Identity);
	case EActAddMoveSpace::World:
	default:
		return FTransform::Identity;
	}
}

int32 UActCharacterMovementComponent::AcquireAddMoveHandle(int32 ExistingHandle, int32 SyncId)
{
	if (ExistingHandle != INDEX_NONE)
	{
		return ExistingHandle;
	}

	if (SyncId != INDEX_NONE)
	{
		if (const int32* ExistingSyncHandle = VelocityAdditionMapBySyncId.Find(SyncId))
		{
			return *ExistingSyncHandle;
		}
	}

	return NextAddMoveHandle++;
}

void UActCharacterMovementComponent::RemoveAddMoveMappings(int32 Handle, const FActVelocityAdditionState* State)
{
	if (!State)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = State->Mesh.Get())
	{
		VelocityAdditionMapByMesh.Remove(Mesh);
	}

	if (State->Params.SyncId != INDEX_NONE)
	{
		if (const int32* SyncHandle = VelocityAdditionMapBySyncId.Find(State->Params.SyncId); SyncHandle && *SyncHandle == Handle)
		{
			VelocityAdditionMapBySyncId.Remove(State->Params.SyncId);
		}
	}
}

void UActCharacterMovementComponent::ApplyPendingAddMove(float DeltaSeconds)
{
	if (bApplyingAddMove || !UpdatedComponent || VelocityAdditionMap.IsEmpty() || DeltaSeconds <= UE_SMALL_NUMBER)
	{
		PendingAddMoveDisplacement = FVector::ZeroVector;
		PendingAddMoveRotationDelta = FQuat::Identity;
		return;
	}

	FQuat RotationDelta = FQuat::Identity;
	const FVector AddMoveDisplacement = ConsumeAddMoveDisplacement(DeltaSeconds, RotationDelta);
	PendingAddMoveDisplacement = AddMoveDisplacement;
	PendingAddMoveRotationDelta = RotationDelta;

	if (PendingAddMoveDisplacement.IsNearlyZero() && PendingAddMoveRotationDelta.Equals(FQuat::Identity))
	{
		return;
	}

	const FQuat NewRotation = PendingAddMoveRotationDelta * UpdatedComponent->GetComponentQuat();
	FHitResult Hit;
	bApplyingAddMove = true;
	// Apply displacement using safe movement so collisions resolve deterministically across prediction/replay.
	SafeMoveUpdatedComponent(PendingAddMoveDisplacement, NewRotation.Rotator(), true, Hit);
	bApplyingAddMove = false;
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(PendingAddMoveDisplacement, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	PendingAddMoveDisplacement = FVector::ZeroVector;
	PendingAddMoveRotationDelta = FQuat::Identity;
}

void UActCharacterMovementComponent::CaptureAddMoveSnapshots(TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& OutSnapshots) const
{
	OutSnapshots.Reset();

	for (const TPair<int32, FActVelocityAdditionState>& Pair : VelocityAdditionMap)
	{
		const FActVelocityAdditionState& State = Pair.Value;
		FActAddMoveSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.LocalHandle = Pair.Key;
		Snapshot.SyncId = State.Params.SyncId;
		Snapshot.Params = State.Params;
		Snapshot.ElapsedTime = State.ElapsedTime;
	}
}

void UActCharacterMovementComponent::RestoreAddMoveSnapshots(const TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& Snapshots, bool bNetworkSynchronizedOnly)
{
	auto AreAddMoveParamsEquivalent = [](const FActAddMoveParams& A, const FActAddMoveParams& B)
	{
		return A.SourceType == B.SourceType &&
			A.Space == B.Space &&
			A.Velocity.Equals(B.Velocity) &&
			FMath::IsNearlyEqual(A.Duration, B.Duration) &&
			A.CurveType == B.CurveType &&
			A.Curve == B.Curve &&
			A.Montage == B.Montage &&
			FMath::IsNearlyEqual(A.StartTrackPosition, B.StartTrackPosition) &&
			FMath::IsNearlyEqual(A.EndTrackPosition, B.EndTrackPosition) &&
			A.bApplyRotation == B.bApplyRotation &&
			A.SyncId == B.SyncId;
	};

	TSet<int32> IncomingSyncIds;
	TSet<int32> IncomingLocalHandles;
	for (const FActAddMoveSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.SyncId != INDEX_NONE)
		{
			IncomingSyncIds.Add(Snapshot.SyncId);
		}
		if (!bNetworkSynchronizedOnly && Snapshot.LocalHandle != INDEX_NONE)
		{
			IncomingLocalHandles.Add(Snapshot.LocalHandle);
		}
	}

	TArray<int32> HandlesToRemove;
	for (const TPair<int32, FActVelocityAdditionState>& Pair : VelocityAdditionMap)
	{
		const FActVelocityAdditionState& State = Pair.Value;
		const bool bTrackBySyncId = State.Params.SyncId != INDEX_NONE;
		const bool bTrackByHandle = !bTrackBySyncId && !bNetworkSynchronizedOnly;
		if ((bTrackBySyncId && !IncomingSyncIds.Contains(State.Params.SyncId)) ||
			(bTrackByHandle && !IncomingLocalHandles.Contains(Pair.Key)))
		{
			HandlesToRemove.Add(Pair.Key);
		}
	}

	// Phase 1: remove any local AddMove entries that do not exist in incoming snapshot set.
	// This keeps both sides in sync and prevents "ghost" motion continuing after prediction is corrected.
	for (const int32 Handle : HandlesToRemove)
	{
		StopAddMove(Handle);
	}

	// Phase 2: add/update incoming entries.
	// If SyncId matches, we update in-place (stable identity across section refresh / branch changes).
	bool bChanged = false;
	for (const FActAddMoveSnapshot& Snapshot : Snapshots)
	{
		int32 ExistingHandle = INDEX_NONE;
		if (Snapshot.SyncId != INDEX_NONE)
		{
			if (const int32* ExistingSyncHandle = VelocityAdditionMapBySyncId.Find(Snapshot.SyncId))
			{
				ExistingHandle = *ExistingSyncHandle;
			}
		}
		else if (!bNetworkSynchronizedOnly)
		{
			ExistingHandle = Snapshot.LocalHandle;
		}

		int32 Handle = ExistingHandle;
		FActVelocityAdditionState* ExistingState = ExistingHandle != INDEX_NONE ? VelocityAdditionMap.Find(ExistingHandle) : nullptr;
		if (!ExistingState || !AreAddMoveParamsEquivalent(ExistingState->Params, Snapshot.Params))
		{
			Handle = SetAddMoveInternal(Snapshot.Params, nullptr, ExistingHandle);
			ExistingState = VelocityAdditionMap.Find(Handle);
		}

		if (FActVelocityAdditionState* State = ExistingState)
		{
			State->ElapsedTime = Snapshot.ElapsedTime;
			bChanged = true;
		}
	}

	if (bChanged)
	{
		++AddMoveStateRevision;
	}
}

void UActCharacterMovementComponent::UpdateAddMoveNetworkCorrectionMode()
{
	const bool bHasAddMove = VelocityAdditionMap.Num() > 0;
	if (bHasAddMove == bAddMoveNetworkCorrectionOverrideActive)
	{
		return;
	}

	if (bHasAddMove)
	{
		// During authored combat motion we prefer "smoothness" over micro-corrections.
		// We temporarily relax correction rules while AddMove is active to avoid jitter cascades.
		// (The more complete solution is full move-data alignment via SavedMove snapshots above;
		// this flagging is an extra guardrail.)
		bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
		bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;
		bIgnoreClientMovementErrorChecksAndCorrection = true;
		bServerAcceptClientAuthoritativePosition = true;
		bAddMoveNetworkCorrectionOverrideActive = true;
		return;
	}

	// Restore original correction settings when combat motion ends.
	bIgnoreClientMovementErrorChecksAndCorrection = bCachedIgnoreClientMovementErrorChecksAndCorrection;
	bServerAcceptClientAuthoritativePosition = bCachedServerAcceptClientAuthoritativePosition;
	bAddMoveNetworkCorrectionOverrideActive = false;
}
