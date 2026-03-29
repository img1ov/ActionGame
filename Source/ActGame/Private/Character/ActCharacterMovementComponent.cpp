// Fill out your copyright notice in the Description page of Project Settings.

#include "Character/ActCharacterMovementComponent.h"

#include "ActLogChannels.h"
#include "Animation/AnimMontage.h"
#include "Character/ActBattleComponent.h"
#include "Character/ActCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameplayTagAssetInterface.h"

namespace ActCharacter
{
	static float GroundTraceDistance = 100000.0f;
}

UActCharacterMovementComponent::UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
	bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;
	bIsOnGround = IsMovingOnGround();

	DefaultMovementStateParams.MaxWalkSpeed = MaxWalkSpeed;
	DefaultMovementStateParams.MaxAcceleration = MaxAcceleration;
	DefaultMovementStateParams.BrakingDecelerationWalking = BrakingDecelerationWalking;
	DefaultMovementStateParams.GroundFriction = GroundFriction;
	DefaultMovementStateParams.RotationRate = RotationRate;
	DefaultMovementStateParams.bOrientRotationToMovement = bOrientRotationToMovement;
	DefaultMovementStateParams.bUseControllerDesiredRotation = bUseControllerDesiredRotation;

	SetNetworkMoveDataContainer(NetworkMoveDataContainer);
}

void UActCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
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
		const ECollisionChannel CollisionChannel = UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn;
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, TraceStart.Z - ActCharacter::GroundTraceDistance - CapsuleHalfHeight);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActCharacterMovementComponent_GetGroundInfo), false, CharacterOwner);
		FCollisionResponseParams ResponseParams;
		InitCollisionParams(QueryParams, ResponseParams);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, CollisionChannel, QueryParams, ResponseParams);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = ActCharacter::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max(HitResult.Distance - CapsuleHalfHeight, 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;
	return CachedGroundInfo;
}

void UActCharacterMovementComponent::ApplyMovementStateParams(const FActMovementStateParams& Params)
{
	MaxWalkSpeed = Params.MaxWalkSpeed;
	MaxAcceleration = Params.MaxAcceleration;
	BrakingDecelerationWalking = Params.BrakingDecelerationWalking;
	GroundFriction = Params.GroundFriction;
	RotationRate = Params.RotationRate;
	bOrientRotationToMovement = Params.bOrientRotationToMovement;
	bUseControllerDesiredRotation = Params.bUseControllerDesiredRotation;
}

void UActCharacterMovementComponent::ResetMovementStateParams()
{
	ApplyMovementStateParams(DefaultMovementStateParams);
}

int32 UActCharacterMovementComponent::PushMovementStateParams(const FActMovementStateParams& Params, const int32 ExistingHandle)
{
	if (ExistingHandle != INDEX_NONE)
	{
		if (FActMovementStateStackEntry* ExistingEntry = MovementStateStack.FindByPredicate(
			[ExistingHandle](const FActMovementStateStackEntry& Entry)
			{
				return Entry.Handle == ExistingHandle;
			}))
		{
			ExistingEntry->Params = Params;
			const FActMovementStateStackEntry UpdatedEntry = *ExistingEntry;
			MovementStateStack.RemoveAll([ExistingHandle](const FActMovementStateStackEntry& Entry) { return Entry.Handle == ExistingHandle; });
			MovementStateStack.Add(UpdatedEntry);
			RefreshMovementStateParamsFromStack();
			return ExistingHandle;
		}
	}

	FActMovementStateStackEntry& NewEntry = MovementStateStack.AddDefaulted_GetRef();
	NewEntry.Handle = NextMovementStateHandle++;
	NewEntry.Params = Params;
	RefreshMovementStateParamsFromStack();
	return NewEntry.Handle;
}

bool UActCharacterMovementComponent::PopMovementStateParams(const int32 Handle)
{
	const int32 RemovedCount = MovementStateStack.RemoveAll(
		[Handle](const FActMovementStateStackEntry& Entry)
		{
			return Entry.Handle == Handle;
		});

	if (RemovedCount <= 0)
	{
		return false;
	}

	RefreshMovementStateParamsFromStack();
	return true;
}

void UActCharacterMovementComponent::ClearMovementStateParamsStack()
{
	MovementStateStack.Reset();
	RefreshMovementStateParamsFromStack();
}

int32 UActCharacterMovementComponent::GenerateAddMoveSyncId()
{
	int32 SyncId = NextGeneratedAddMoveSyncId++;
	if (SyncId == INDEX_NONE)
	{
		SyncId = NextGeneratedAddMoveSyncId++;
	}

	return SyncId;
}

int32 UActCharacterMovementComponent::SetAddMoveWorld(FVector WorldVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle, int32 SyncId)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::World;
	Params.Velocity = WorldVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	Params.SyncId = SyncId;
	return ApplyAddMoveParams(Params, nullptr, ExistingHandle);
}

int32 UActCharacterMovementComponent::SetAddMove(FVector LocalVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle, int32 SyncId)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::Actor;
	Params.Velocity = LocalVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	Params.SyncId = SyncId;
	return ApplyAddMoveParams(Params, nullptr, ExistingHandle);
}

int32 UActCharacterMovementComponent::SetAddMoveWithMesh(USkeletalMeshComponent* Mesh, FVector MeshLocalVelocity, float Duration, EActAddMoveCurveType CurveType, UCurveFloat* Curve, int32 ExistingHandle, int32 SyncId)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::Velocity;
	Params.Space = EActAddMoveSpace::Mesh;
	Params.Velocity = MeshLocalVelocity;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = Curve;
	Params.SyncId = SyncId;
	return ApplyAddMoveParams(Params, Mesh, ExistingHandle);
}

int32 UActCharacterMovementComponent::ApplyAddMoveParams(const FActAddMoveParams& Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle)
{
	return SetAddMoveInternal(Params, Mesh, ExistingHandle, true);
}

int32 UActCharacterMovementComponent::SetAddMoveFromRootMotionRange(
	UAnimMontage* Montage,
	float StartTrackPosition,
	float EndTrackPosition,
	float Duration,
	bool bApplyRotation,
	bool bRespectAddMoveRotation,
	bool bIgnoreZAccumulate,
	int32 ExistingHandle,
	int32 SyncId)
{
	FActAddMoveParams Params;
	Params.SourceType = EActAddMoveSourceType::MontageRootMotion;
	Params.Space = EActAddMoveSpace::Actor;
	Params.Duration = Duration;
	Params.Montage = Montage;
	Params.StartTrackPosition = StartTrackPosition;
	Params.EndTrackPosition = EndTrackPosition;
	Params.bApplyRotation = bApplyRotation;
	Params.bRespectAddMoveRotation = bRespectAddMoveRotation;
	Params.bIgnoreZAccumulate = bIgnoreZAccumulate;
	Params.SyncId = SyncId;
	return ApplyAddMoveParams(Params, nullptr, ExistingHandle);
}

bool UActCharacterMovementComponent::StopAddMove(int32 Handle)
{
	return StopAddMoveInternal(Handle, true);
}

bool UActCharacterMovementComponent::StopAddMoveWithMesh(USkeletalMeshComponent* Mesh)
{
	if (!Mesh)
	{
		return false;
	}

	if (const int32* Handle = AddMoveStateMapByMesh.Find(Mesh))
	{
		return StopAddMoveInternal(*Handle, true);
	}

	return false;
}

bool UActCharacterMovementComponent::StopAddMoveBySyncId(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return false;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		return StopAddMoveInternal(*Handle, true);
	}

	return false;
}

bool UActCharacterMovementComponent::PauseAddMove(const int32 Handle)
{
	const FActAddMoveState* State = AddMoveStateMap.Find(Handle);
	const int32 SyncId = State ? State->Params.SyncId : INDEX_NONE;
	const bool bPaused = PauseAddMoveInternal(Handle);

	if (bPaused && SyncId != INDEX_NONE)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			ActCharacter->MulticastPauseAddMove(SyncId);
			ActCharacter->ForceNetUpdate();
		}
	}

	return bPaused;
}

bool UActCharacterMovementComponent::ResumeAddMove(const int32 Handle)
{
	const FActAddMoveState* State = AddMoveStateMap.Find(Handle);
	const int32 SyncId = State ? State->Params.SyncId : INDEX_NONE;
	const bool bResumed = ResumeAddMoveInternal(Handle);

	if (bResumed && SyncId != INDEX_NONE)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			ActCharacter->MulticastResumeAddMove(SyncId);
			ActCharacter->ForceNetUpdate();
		}
	}

	return bResumed;
}

bool UActCharacterMovementComponent::PauseAddMoveBySyncId(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return false;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		return PauseAddMove(*Handle);
	}

	return false;
}

bool UActCharacterMovementComponent::ResumeAddMoveBySyncId(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return false;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		return ResumeAddMove(*Handle);
	}

	return false;
}

void UActCharacterMovementComponent::StopAllAddMove()
{
	TArray<int32> SyncIdsToBroadcast;
	SyncIdsToBroadcast.Reserve(AddMoveStateMap.Num());
	for (const TPair<int32, FActAddMoveState>& Pair : AddMoveStateMap)
	{
		if (Pair.Value.Params.SyncId != INDEX_NONE)
		{
			SyncIdsToBroadcast.AddUnique(Pair.Value.Params.SyncId);
		}
	}

	AddMoveStateMap.Empty();
	AddMoveStateMapByMesh.Empty();
	AddMoveStateMapBySyncId.Empty();
	PendingAddMoveDisplacement = FVector::ZeroVector;
	PendingAddMoveRotationDelta = FQuat::Identity;
	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();

	if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
	{
		for (const int32 SyncId : SyncIdsToBroadcast)
		{
			ActCharacter->MulticastStopAddMove(SyncId);
		}
		ActCharacter->ForceNetUpdate();
	}
}

bool UActCharacterMovementComponent::HasActiveAddMove() const
{
	return AddMoveStateMap.Num() > 0;
}

bool UActCharacterMovementComponent::HasAddMoveBySyncId(const int32 SyncId) const
{
	return AddMoveStateMapBySyncId.Contains(SyncId);
}

void UActCharacterMovementComponent::SetAddMoveRotationToActor(AActor* Target, EActAddMoveRotationActorMode ActorMode, float InRotationRate, float AcceptableYawError, bool bClearOnReached, int32 SyncId)
{
	FActAddMoveRotationParams Params;
	Params.SourceType = EActAddMoveRotationSourceType::Actor;
	Params.TargetActor = Target;
	Params.ActorMode = ActorMode;
	Params.RotationRate = FMath::Max(0.0f, InRotationRate);
	Params.AcceptableYawError = FMath::Max(0.0f, AcceptableYawError);
	Params.bClearOnReached = bClearOnReached;
	Params.SyncId = SyncId;
	SetAddMoveRotationInternal(Params, true);
}

void UActCharacterMovementComponent::SetAddMoveRotationToDirection(FVector WorldDirection, float InRotationRate, float AcceptableYawError, bool bClearOnReached, int32 SyncId)
{
	WorldDirection.Z = 0.0f;

	FActAddMoveRotationParams Params;
	Params.SourceType = EActAddMoveRotationSourceType::Direction;
	Params.Direction = WorldDirection.GetSafeNormal();
	Params.RotationRate = FMath::Max(0.0f, InRotationRate);
	Params.AcceptableYawError = FMath::Max(0.0f, AcceptableYawError);
	Params.bClearOnReached = bClearOnReached;
	Params.SyncId = SyncId;
	SetAddMoveRotationInternal(Params, true);
}

void UActCharacterMovementComponent::ClearAddMoveRotation()
{
	ClearAddMoveRotationInternal(true);
}

bool UActCharacterMovementComponent::HasActiveAddMoveRotation() const
{
	return GetAddMoveRotationParams() != nullptr;
}

const FActAddMoveRotationParams* UActCharacterMovementComponent::GetAddMoveRotationParams() const
{
	if (!AddMoveRotationParams.IsValidRequest())
	{
		return nullptr;
	}

	if (AddMoveRotationParams.SourceType == EActAddMoveRotationSourceType::Direction && AddMoveRotationParams.Direction.IsNearlyZero())
	{
		return nullptr;
	}

	return &AddMoveRotationParams;
}

void UActCharacterMovementComponent::ApplyReplicatedAddMove(const FActAddMoveParams& Params)
{
	// Owner-side predicted abilities may already have created the same SyncId-backed AddMove locally.
	// Do not restart it from server multicast or the client will replay the startup displacement.
	if (Params.SyncId != INDEX_NONE && CharacterOwner && CharacterOwner->IsLocallyControlled() && HasAddMoveBySyncId(Params.SyncId))
	{
		return;
	}

	USkeletalMeshComponent* Mesh = nullptr;
	if (Params.Space == EActAddMoveSpace::Mesh && CharacterOwner)
	{
		Mesh = CharacterOwner->GetMesh();
	}

	SetAddMoveInternal(Params, Mesh, INDEX_NONE, false);
}

void UActCharacterMovementComponent::StopReplicatedAddMove(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		StopAddMoveInternal(*Handle, false);
	}
}

void UActCharacterMovementComponent::PauseReplicatedAddMove(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		if (FActAddMoveState* State = AddMoveStateMap.Find(*Handle))
		{
			// Replicated pause is absolute state sync, not a new gameplay-side pause request.
			if (State->PauseLockCount <= 0)
			{
				PauseAddMoveInternal(*Handle);
			}
		}
	}
}

void UActCharacterMovementComponent::ResumeReplicatedAddMove(const int32 SyncId)
{
	if (SyncId == INDEX_NONE)
	{
		return;
	}

	if (const int32* Handle = AddMoveStateMapBySyncId.Find(SyncId))
	{
		if (FActAddMoveState* State = AddMoveStateMap.Find(*Handle))
		{
			// Resume from replication clears the replicated pause state instead of decrementing gameplay locks.
			if (State->PauseLockCount > 0)
			{
				State->PauseLockCount = 0;
				++AddMoveStateRevision;
			}
		}
	}
}

void UActCharacterMovementComponent::ApplyReplicatedAddMoveRotation(const FActAddMoveRotationParams& Params)
{
	if (Params.SyncId != INDEX_NONE &&
		CharacterOwner &&
		CharacterOwner->IsLocallyControlled() &&
		GetAddMoveRotationParams() &&
		GetAddMoveRotationParams()->SyncId == Params.SyncId)
	{
		return;
	}

	SetAddMoveRotationInternal(Params, false);
}

void UActCharacterMovementComponent::ClearReplicatedAddMoveRotation()
{
	ClearAddMoveRotationInternal(false);
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
		// Server move RPCs only carry network-identified snapshots, so restore them before simulation.
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
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

void UActCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	UCharacterMovementComponent::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	const bool bOldHasAcceleration = bHasAcceleration;
	const bool bOldIsOnGround = bIsOnGround;
	bHasAcceleration = GetHasAcceleration();
	bIsOnGround = IsMovingOnGround();

	if (bHasAcceleration != bOldHasAcceleration)
	{
		OnAccelerationStateChanged.Broadcast(bOldHasAcceleration, bHasAcceleration);
	}

	if (bIsOnGround != bOldIsOnGround)
	{
		OnGroundStateChanged.Broadcast(bOldIsOnGround, bIsOnGround);
	}

	ApplyPendingAddMove(DeltaSeconds);

	// Anim root motion can own yaw after PhysicsRotation is skipped. Re-apply explicit rotation
	// AddMove here so hit-react / attack facing still works during authored RM montages.
	if (HasAnimRootMotion() && GetAddMoveRotationParams())
	{
		TryApplyAddMoveRotation(DeltaSeconds);
	}
}

void UActCharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	if (TryApplyAddMoveRotation(DeltaTime))
	{
		return;
	}

	const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(CharacterOwner);
	const bool bStrafeMoveActive = TagInterface && TagInterface->HasMatchingGameplayTag(Event_Movement_StrafeMove);
	if (bStrafeMoveActive)
	{
		TryApplyStrafeRotation(DeltaTime);
		return;
	}

	Super::PhysicsRotation(DeltaTime);
}

bool UActCharacterMovementComponent::TryApplyAddMoveRotation(float DeltaTime)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const FActAddMoveRotationParams* RotationParams = GetAddMoveRotationParams();
	if (!RotationParams)
	{
		return false;
	}

	FVector DesiredDirection = FVector::ZeroVector;
	switch (RotationParams->SourceType)
	{
	case EActAddMoveRotationSourceType::Actor:
		{
			AActor* TargetActor = RotationParams->TargetActor.Get();
			if (!TargetActor)
			{
				ClearAddMoveRotationInternal(false);
				return false;
			}

			switch (RotationParams->ActorMode)
			{
			case EActAddMoveRotationActorMode::MatchTargetForward:
				DesiredDirection = TargetActor->GetActorForwardVector();
				break;
			case EActAddMoveRotationActorMode::MatchOppositeTargetForward:
				DesiredDirection = -TargetActor->GetActorForwardVector();
				break;
			case EActAddMoveRotationActorMode::FaceTarget:
				DesiredDirection = TargetActor->GetActorLocation() - CharacterOwner->GetActorLocation();
				break;
			case EActAddMoveRotationActorMode::BackToTarget:
				DesiredDirection = CharacterOwner->GetActorLocation() - TargetActor->GetActorLocation();
				break;
			default:
				break;
			}
		}
		break;
	case EActAddMoveRotationSourceType::Direction:
		DesiredDirection = RotationParams->Direction;
		break;
	default:
		break;
	}

	DesiredDirection.Z = 0.0f;
	if (DesiredDirection.IsNearlyZero())
	{
		if (RotationParams->SourceType == EActAddMoveRotationSourceType::Direction)
		{
			ClearAddMoveRotationInternal(false);
		}
		return false;
	}

	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	FRotator DesiredRotation = DesiredDirection.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	const float YawRate = RotationParams->RotationRate > UE_SMALL_NUMBER ? RotationParams->RotationRate : RotationRate.Yaw;
	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, DeltaTime, YawRate);
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);

	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRotation.Yaw, DesiredRotation.Yaw));
	if (RotationParams->bClearOnReached && YawError <= RotationParams->AcceptableYawError)
	{
		ClearAddMoveRotationInternal(false);
	}

	return true;
}

bool UActCharacterMovementComponent::TryApplyStrafeRotation(float DeltaTime)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return false;
	}

	const bool bHasMoveInput = GetCurrentAcceleration().SizeSquared2D() > KINDA_SMALL_NUMBER || Velocity.SizeSquared2D() > KINDA_SMALL_NUMBER;
	if (!bHasMoveInput)
	{
		return false;
	}

	UActBattleComponent* BattleComponent = CharacterOwner->FindComponentByClass<UActBattleComponent>();
	AActor* LockOnTarget = BattleComponent ? BattleComponent->GetLockOnTarget() : nullptr;
	if (!LockOnTarget)
	{
		return false;
	}

	const FVector ToTarget = LockOnTarget->GetActorLocation() - CharacterOwner->GetActorLocation();
	if (ToTarget.IsNearlyZero())
	{
		return false;
	}

	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	FRotator DesiredRotation = ToTarget.Rotation();
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	const FRotator NewRotation = FMath::RInterpConstantTo(CurrentRotation, DesiredRotation, DeltaTime, RotationRate.Yaw);
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);
	return true;
}

int32 UActCharacterMovementComponent::SetAddMoveInternal(FActAddMoveParams Params, USkeletalMeshComponent* Mesh, int32 ExistingHandle, const bool bBroadcastToSimulatedProxies)
{
	if (Params.Duration == 0.0f)
	{
		return INDEX_NONE;
	}

	if (Params.SyncId == INDEX_NONE)
	{
		// Authority-generated SyncIds give Blueprint-authored server moves a stable identity for
		// simulated-proxy bootstrap and owner/server replay alignment.
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			Params.SyncId = GenerateAddMoveSyncId();
		}
	}

	const int32 Handle = AcquireAddMoveHandle(ExistingHandle, Params.SyncId);

	FActAddMoveState& State = AddMoveStateMap.FindOrAdd(Handle);
	RemoveAddMoveMappings(Handle, &State);
	State.Handle = Handle;
	State.Params = Params;
	State.Mesh = Mesh;
	State.ElapsedTime = 0.0f;

	if (Mesh)
	{
		AddMoveStateMapByMesh.FindOrAdd(Mesh) = Handle;
	}
	if (Params.SyncId != INDEX_NONE)
	{
		AddMoveStateMapBySyncId.FindOrAdd(Params.SyncId) = Handle;
	}

	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();

	if (bBroadcastToSimulatedProxies)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			// Simulated proxies do not execute the gameplay task graph, so bootstrap the authored AddMove explicitly.
			ActCharacter->MulticastBootstrapAddMove(State.Params);
			ActCharacter->ForceNetUpdate();
		}
	}

	return Handle;
}

bool UActCharacterMovementComponent::StopAddMoveInternal(const int32 Handle, const bool bBroadcastToSimulatedProxies)
{
	if (Handle == INDEX_NONE)
	{
		return false;
	}

	const FActAddMoveState* RemovedState = AddMoveStateMap.Find(Handle);
	const int32 SyncId = RemovedState ? RemovedState->Params.SyncId : INDEX_NONE;
	RemoveAddMoveMappings(Handle, RemovedState);

	const bool bRemoved = AddMoveStateMap.Remove(Handle) > 0;
	if (!bRemoved)
	{
		return false;
	}

	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();

	if (bBroadcastToSimulatedProxies && SyncId != INDEX_NONE)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			ActCharacter->MulticastStopAddMove(SyncId);
			ActCharacter->ForceNetUpdate();
		}
	}

	return true;
}

bool UActCharacterMovementComponent::PauseAddMoveInternal(const int32 Handle)
{
	if (FActAddMoveState* State = AddMoveStateMap.Find(Handle))
	{
		++State->PauseLockCount;
		++AddMoveStateRevision;
		return true;
	}

	return false;
}

bool UActCharacterMovementComponent::ResumeAddMoveInternal(const int32 Handle)
{
	if (FActAddMoveState* State = AddMoveStateMap.Find(Handle))
	{
		const int32 NewPauseLockCount = FMath::Max(0, State->PauseLockCount - 1);
		if (NewPauseLockCount != State->PauseLockCount)
		{
			State->PauseLockCount = NewPauseLockCount;
			++AddMoveStateRevision;
		}
		return true;
	}

	return false;
}

FVector UActCharacterMovementComponent::ConsumeAddMoveDisplacement(float DeltaSeconds, FQuat& OutRotationDelta)
{
	FVector TotalDisplacement = FVector::ZeroVector;
	OutRotationDelta = FQuat::Identity;

	TArray<int32> HandlesToRemove;
	HandlesToRemove.Reserve(AddMoveStateMap.Num());
	for (TPair<int32, FActAddMoveState>& Pair : AddMoveStateMap)
	{
		const int32 Handle = Pair.Key;
		FActAddMoveState& State = Pair.Value;

		if (State.PauseLockCount > 0)
		{
			// Paused AddMove keeps runtime state but contributes no displacement/time this frame.
			continue;
		}

		if (State.Params.SourceType == EActAddMoveSourceType::Velocity &&
			State.Params.Space == EActAddMoveSpace::Mesh &&
			!State.Mesh.IsValid())
		{
			HandlesToRemove.Add(Handle);
			continue;
		}

		if (!DoesAddMovePassModeFilter(State.Params))
		{
			// Mode-gated AddMove is considered finished once its movement-mode requirement no longer matches.
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
		const FVector AddDisplacement = ResolveAddMoveDisplacement(
			State.Params,
			State.Mesh.Get(),
			PreviousElapsedTime,
			NewElapsedTime,
			EffectiveDeltaTime,
			bHasRotationDelta,
			RotationDelta);

		if (AddDisplacement.ContainsNaN())
		{
			UE_LOG(LogAct, Warning, TEXT("Removing invalid AddMove. Owner=%s Handle=%d SyncId=%d ParamsSource=%d"),
				*GetNameSafe(CharacterOwner),
				Handle,
				State.Params.SyncId,
				static_cast<int32>(State.Params.SourceType));
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

	RemoveAddMoves(HandlesToRemove);
	return TotalDisplacement;
}

float UActCharacterMovementComponent::EvaluateAddMoveScale(const FActAddMoveParams& Params, const float ElapsedTime, const float Duration) const
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

bool UActCharacterMovementComponent::DoesAddMovePassModeFilter(const FActAddMoveParams& Params) const
{
	switch (Params.ModeFilter)
	{
	case EActAddMoveModeFilter::Any:
		return true;
	case EActAddMoveModeFilter::GroundedOnly:
		return IsMovingOnGround();
	case EActAddMoveModeFilter::AirOnly:
		return !IsMovingOnGround();
	case EActAddMoveModeFilter::FallingOnly:
		return MovementMode == MOVE_Falling;
	case EActAddMoveModeFilter::WalkingOnly:
		return MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking;
	default:
		return true;
	}
}

FVector UActCharacterMovementComponent::ResolveAddMoveDisplacement(
	const FActAddMoveParams& Params,
	USkeletalMeshComponent* Mesh,
	float PreviousElapsedTime,
	float NewElapsedTime,
	float DeltaSeconds,
	bool& bOutHasRotationDelta,
	FQuat& OutRotationDelta) const
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

		// Explicit gameplay-facing should win over extracted montage yaw when requested.
		const bool bShouldApplyAddMoveRotation = Params.bApplyRotation && (!Params.bRespectAddMoveRotation || GetAddMoveRotationParams() == nullptr);
		if (!bShouldApplyAddMoveRotation)
		{
			DeltaTransform.SetRotation(FQuat::Identity);
		}

		if (bShouldApplyAddMoveRotation)
		{
			bOutHasRotationDelta = !DeltaTransform.GetRotation().Equals(FQuat::Identity);
			OutRotationDelta = DeltaTransform.GetRotation();
		}

		if (Params.bIgnoreZAccumulate)
		{
			FVector Translation = DeltaTransform.GetTranslation();
			Translation.Z = 0.0f;
			DeltaTransform.SetTranslation(Translation);
		}

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

int32 UActCharacterMovementComponent::AcquireAddMoveHandle(const int32 ExistingHandle, const int32 SyncId)
{
	if (ExistingHandle != INDEX_NONE)
	{
		return ExistingHandle;
	}

	if (SyncId != INDEX_NONE)
	{
		if (const int32* ExistingSyncHandle = AddMoveStateMapBySyncId.Find(SyncId))
		{
			return *ExistingSyncHandle;
		}
	}

	return NextAddMoveHandle++;
}

void UActCharacterMovementComponent::ApplyPendingAddMove(float DeltaSeconds)
{
	if (bApplyingAddMove || !UpdatedComponent || AddMoveStateMap.IsEmpty() || DeltaSeconds <= UE_SMALL_NUMBER)
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
	SafeMoveUpdatedComponent(PendingAddMoveDisplacement, NewRotation.Rotator(), true, Hit);
	bApplyingAddMove = false;
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(PendingAddMoveDisplacement, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	PendingAddMoveDisplacement = FVector::ZeroVector;
	PendingAddMoveRotationDelta = FQuat::Identity;
}

void UActCharacterMovementComponent::RemoveAddMoveMappings(const int32 Handle, const FActAddMoveState* State)
{
	if (!State)
	{
		return;
	}

	if (USkeletalMeshComponent* Mesh = State->Mesh.Get())
	{
		AddMoveStateMapByMesh.Remove(Mesh);
	}

	if (State->Params.SyncId != INDEX_NONE)
	{
		if (const int32* SyncHandle = AddMoveStateMapBySyncId.Find(State->Params.SyncId); SyncHandle && *SyncHandle == Handle)
		{
			AddMoveStateMapBySyncId.Remove(State->Params.SyncId);
		}
	}
}

void UActCharacterMovementComponent::RemoveAddMoves(const TArray<int32>& Handles)
{
	if (Handles.IsEmpty())
	{
		return;
	}

	bool bRemovedAny = false;
	for (const int32 Handle : Handles)
	{
		if (Handle == INDEX_NONE)
		{
			continue;
		}

		const FActAddMoveState* State = AddMoveStateMap.Find(Handle);
		RemoveAddMoveMappings(Handle, State);
		bRemovedAny |= AddMoveStateMap.Remove(Handle) > 0;
	}

	if (!bRemovedAny)
	{
		return;
	}

	++AddMoveStateRevision;
	UpdateAddMoveNetworkCorrectionMode();
}

void UActCharacterMovementComponent::CaptureAddMoveSnapshots(TArray<FActAddMoveSnapshot, TInlineAllocator<4>>& OutSnapshots) const
{
	OutSnapshots.Reset();
	OutSnapshots.Reserve(AddMoveStateMap.Num());

	for (const TPair<int32, FActAddMoveState>& Pair : AddMoveStateMap)
	{
		const FActAddMoveState& State = Pair.Value;
		FActAddMoveSnapshot& Snapshot = OutSnapshots.AddDefaulted_GetRef();
		Snapshot.LocalHandle = Pair.Key;
		Snapshot.SyncId = State.Params.SyncId;
		Snapshot.Params = State.Params;
		Snapshot.ElapsedTime = State.ElapsedTime;
		Snapshot.bPaused = State.PauseLockCount > 0;
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
			A.bRespectAddMoveRotation == B.bRespectAddMoveRotation &&
			A.bIgnoreZAccumulate == B.bIgnoreZAccumulate &&
			A.ModeFilter == B.ModeFilter &&
			A.SyncId == B.SyncId;
	};

	TArray<int32, TInlineAllocator<4>> IncomingSyncIds;
	TArray<int32, TInlineAllocator<4>> IncomingLocalHandles;
	IncomingSyncIds.Reserve(Snapshots.Num());
	IncomingLocalHandles.Reserve(Snapshots.Num());
	for (const FActAddMoveSnapshot& Snapshot : Snapshots)
	{
		if (Snapshot.SyncId != INDEX_NONE)
		{
			IncomingSyncIds.AddUnique(Snapshot.SyncId);
		}
		if (!bNetworkSynchronizedOnly && Snapshot.LocalHandle != INDEX_NONE)
		{
			IncomingLocalHandles.AddUnique(Snapshot.LocalHandle);
		}
	}

	TArray<int32> HandlesToRemove;
	HandlesToRemove.Reserve(AddMoveStateMap.Num());
	for (const TPair<int32, FActAddMoveState>& Pair : AddMoveStateMap)
	{
		const FActAddMoveState& State = Pair.Value;
		const bool bTrackBySyncId = State.Params.SyncId != INDEX_NONE;
		const bool bTrackByHandle = !bTrackBySyncId && !bNetworkSynchronizedOnly;
		if ((bTrackBySyncId && !IncomingSyncIds.Contains(State.Params.SyncId)) ||
			(bTrackByHandle && !IncomingLocalHandles.Contains(Pair.Key)))
		{
			HandlesToRemove.Add(Pair.Key);
		}
	}

	RemoveAddMoves(HandlesToRemove);

	bool bChanged = false;
	for (const FActAddMoveSnapshot& Snapshot : Snapshots)
	{
		if (bNetworkSynchronizedOnly)
		{
			if (Snapshot.SyncId == INDEX_NONE)
			{
				continue;
			}

			if (const int32* ExistingSyncHandle = AddMoveStateMapBySyncId.Find(Snapshot.SyncId))
			{
				if (FActAddMoveState* State = AddMoveStateMap.Find(*ExistingSyncHandle))
				{
					State->ElapsedTime = Snapshot.ElapsedTime;
					State->PauseLockCount = Snapshot.bPaused ? 1 : 0;
					bChanged = true;
				}
			}

			continue;
		}

		int32 ExistingHandle = INDEX_NONE;
		if (Snapshot.SyncId != INDEX_NONE)
		{
			if (const int32* ExistingSyncHandle = AddMoveStateMapBySyncId.Find(Snapshot.SyncId))
			{
				ExistingHandle = *ExistingSyncHandle;
			}
		}
		else
		{
			ExistingHandle = Snapshot.LocalHandle;
		}

		int32 Handle = ExistingHandle;
		FActAddMoveState* ExistingState = ExistingHandle != INDEX_NONE ? AddMoveStateMap.Find(ExistingHandle) : nullptr;
		if (!ExistingState || !AreAddMoveParamsEquivalent(ExistingState->Params, Snapshot.Params))
		{
			Handle = SetAddMoveInternal(Snapshot.Params, nullptr, ExistingHandle, false);
			ExistingState = AddMoveStateMap.Find(Handle);
		}

		if (FActAddMoveState* State = ExistingState)
		{
			State->ElapsedTime = Snapshot.ElapsedTime;
			State->PauseLockCount = Snapshot.bPaused ? 1 : 0;
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
	const bool bHasAddMove = !AddMoveStateMap.IsEmpty();
	if (bHasAddMove == bAddMoveNetworkCorrectionOverrideActive)
	{
		return;
	}

	if (bHasAddMove)
	{
		bCachedIgnoreClientMovementErrorChecksAndCorrection = bIgnoreClientMovementErrorChecksAndCorrection;
		bCachedServerAcceptClientAuthoritativePosition = bServerAcceptClientAuthoritativePosition;
		bIgnoreClientMovementErrorChecksAndCorrection = true;
		bServerAcceptClientAuthoritativePosition = true;
		bAddMoveNetworkCorrectionOverrideActive = true;
		return;
	}

	bIgnoreClientMovementErrorChecksAndCorrection = bCachedIgnoreClientMovementErrorChecksAndCorrection;
	bServerAcceptClientAuthoritativePosition = bCachedServerAcceptClientAuthoritativePosition;
	bAddMoveNetworkCorrectionOverrideActive = false;
}

void UActCharacterMovementComponent::SetAddMoveRotationInternal(const FActAddMoveRotationParams& Params, const bool bBroadcastToSimulatedProxies)
{
	if (!Params.IsValidRequest())
	{
		ClearAddMoveRotationInternal(bBroadcastToSimulatedProxies);
		return;
	}

	AddMoveRotationParams = Params;
	if (AddMoveRotationParams.SourceType == EActAddMoveRotationSourceType::Direction)
	{
		AddMoveRotationParams.Direction.Z = 0.0f;
		AddMoveRotationParams.Direction = AddMoveRotationParams.Direction.GetSafeNormal();
	}

	if (bBroadcastToSimulatedProxies)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			ActCharacter->MulticastSetAddMoveRotation(AddMoveRotationParams);
			ActCharacter->ForceNetUpdate();
		}
	}
}

void UActCharacterMovementComponent::ClearAddMoveRotationInternal(const bool bBroadcastToSimulatedProxies)
{
	const bool bHadRotation = GetAddMoveRotationParams() != nullptr;
	AddMoveRotationParams = FActAddMoveRotationParams();

	if (bHadRotation && bBroadcastToSimulatedProxies)
	{
		if (AActCharacter* ActCharacter = CharacterOwner ? Cast<AActCharacter>(CharacterOwner) : nullptr; ActCharacter && ActCharacter->HasAuthority())
		{
			ActCharacter->MulticastClearAddMoveRotation();
			ActCharacter->ForceNetUpdate();
		}
	}
}

void UActCharacterMovementComponent::RefreshMovementStateParamsFromStack()
{
	if (MovementStateStack.IsEmpty())
	{
		ResetMovementStateParams();
		return;
	}

	ApplyMovementStateParams(MovementStateStack.Last().Params);
}
