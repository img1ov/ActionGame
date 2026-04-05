#include "Character/ActCharacterMovementComponent.h"

#include "Character/ActBattleComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

namespace ActCharacter
{
	static float GroundTraceDistance = 100000.0f;
}

UActCharacterMovementComponent::UActCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
	bNetworkAlwaysReplicateTransformUpdateTimestamp = true;

	DefaultMovementStateParams.MaxWalkSpeed = MaxWalkSpeed;
	DefaultMovementStateParams.MaxAcceleration = MaxAcceleration;
	DefaultMovementStateParams.BrakingDecelerationWalking = BrakingDecelerationWalking;
	DefaultMovementStateParams.GroundFriction = GroundFriction;
	DefaultMovementStateParams.RotationRate = RotationRate;
	DefaultMovementStateParams.bOrientRotationToMovement = bOrientRotationToMovement;
	DefaultMovementStateParams.bUseControllerDesiredRotation = bUseControllerDesiredRotation;

	SetNetworkMoveDataContainer(NetworkMoveDataContainer);
}

UActCharacterMovementComponent* UActCharacterMovementComponent::ResolveActMovementComponent(const AActor* AvatarActor)
{
	const ACharacter* Character = Cast<ACharacter>(AvatarActor);
	return Character ? Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
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

void UActCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
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

}

void UActCharacterMovementComponent::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel)
{
	if (const FActCharacterNetworkMoveData* ActMoveData = static_cast<const FActCharacterNetworkMoveData*>(GetCurrentNetworkMoveData()))
	{
		ApplyNetworkMoveData(ActMoveData->AddMoveState, ActMoveData->AddRotationState);
	}

	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

const FActCharacterGroundInfo& UActCharacterMovementComponent::GetGroundInfo()
{
	ACharacter* Character = CharacterOwner.Get();
	if (!Character || (GFrameCounter == CachedGroundInfo.LastUpdateFrame))
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
		const UCapsuleComponent* CapsuleComp = Character->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn;
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(TraceStart.X, TraceStart.Y, TraceStart.Z - ActCharacter::GroundTraceDistance - CapsuleHalfHeight);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ActCharacterMovementComponent_GetGroundInfo), false, Character);
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

void UActCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	UCharacterMovementComponent::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	
	ApplyAddMove(DeltaSeconds);
}

bool UActCharacterMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
}

void UActCharacterMovementComponent::PhysicsRotation(const float DeltaSeconds)
{
	if (TryApplyAddRotation(DeltaSeconds))
	{
		return;
	}

	if (!bLockOnStrafeActive)
	{
		Super::PhysicsRotation(DeltaSeconds);
		return;
	}

	ApplyLockOnStrafeRotation(DeltaSeconds);
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

void UActCharacterMovementComponent::SetLockOnStrafeActive(const bool bActive)
{
	if (bLockOnStrafeActive == bActive)
	{
		return;
	}

	// Lock-on strafe only affects facing, not the movement direction.
	bLockOnStrafeActive = bActive;
}

AActor* UActCharacterMovementComponent::ResolveLockOnStrafeTarget() const
{
	const ACharacter* Character = CharacterOwner.Get();
	if (!Character)
	{
		return nullptr;
	}

	if (const UActBattleComponent* BattleComponent = Character->FindComponentByClass<UActBattleComponent>())
	{
		return BattleComponent->GetLockOnTarget();
	}

	return nullptr;
}

FRotator UActCharacterMovementComponent::GetLockOnStrafeDesiredRotation(const AActor* Target) const
{
	const ACharacter* Character = CharacterOwner.Get();
	if (!Character)
	{
		return FRotator::ZeroRotator;
	}

	// If no valid target, keep facing the current forward direction.
	if (!IsValid(Target))
	{
		return Character->GetActorRotation();
	}

	FVector ToTarget = Target->GetActorLocation() - Character->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (ToTarget.IsNearlyZero())
	{
		return Character->GetActorRotation();
	}

	return ToTarget.Rotation();
}

void UActCharacterMovementComponent::ApplyLockOnStrafeRotation(const float DeltaSeconds)
{
	if (!CharacterOwner || !UpdatedComponent)
	{
		return;
	}

	// Grab target first, then only rotate while we have both velocity and acceleration.
	const AActor* Target = ResolveLockOnStrafeTarget();
	const bool bHasVelocity = Velocity.SizeSquared() > KINDA_SMALL_NUMBER;
	const bool bHasAccelInput = GetCurrentAcceleration().SizeSquared() > KINDA_SMALL_NUMBER;
	if (!bHasVelocity || !bHasAccelInput)
	{
		Super::PhysicsRotation(DeltaSeconds);
		return;
	}

	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	const FRotator DesiredRotation = GetLockOnStrafeDesiredRotation(Target);
	const FRotator DeltaRot = GetDeltaRotation(DeltaSeconds);

	FRotator NewRotation = CurrentRotation;
	NewRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);

	// Only adjust facing; movement direction remains driven by the controller.
	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);
}

void UActCharacterMovementComponent::SetAddMove(const FVector& MoveVelocity, const bool bIgnoreGravity)
{
	if (MoveVelocity.IsNearlyZero())
	{
		ResetAddMoveState();
		return;
	}

	AddMoveState.bActive = true;
	AddMoveState.Velocity = MoveVelocity;
	AddMoveState.Duration = -1.0f;
	AddMoveState.ElapsedTime = 0.0f;
	AddMoveState.bIgnoreGravity = bIgnoreGravity;

	++AddMoveStateRevision;
	UpdateGravityOverride();
}

void UActCharacterMovementComponent::SetAddMoveConstantForce(const FVector& MoveVelocity, const float Duration, const bool bIgnoreGravity)
{
	if (Duration <= 0.0f || MoveVelocity.IsNearlyZero())
	{
		ResetAddMoveState();
		return;
	}

	AddMoveState.bActive = true;
	AddMoveState.Velocity = MoveVelocity;
	AddMoveState.Duration = Duration;
	AddMoveState.ElapsedTime = 0.0f;
	AddMoveState.bIgnoreGravity = bIgnoreGravity;

	++AddMoveStateRevision;
	UpdateGravityOverride();
}

void UActCharacterMovementComponent::SetAddRotation(const FVector& Direction, const float InRotationRate)
{
	FVector SafeDirection = Direction;
	SafeDirection.Z = 0.0f;
	if (SafeDirection.IsNearlyZero())
	{
		ResetAddRotationState();
		return;
	}

	AddRotationState.bActive = true;
	AddRotationState.bUseDirection = true;
	AddRotationState.Direction = SafeDirection.GetSafeNormal();
	AddRotationState.Rotation = FRotator::ZeroRotator;
	AddRotationState.RotationRate = InRotationRate;

	if (InRotationRate < 0.0f && UpdatedComponent)
	{
		const FRotator DesiredRotation = AddRotationState.Direction.Rotation();
		MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
		AddRotationState = FActAddRotationNetworkState();
		++AddMoveStateRevision;
		return;
	}

	++AddMoveStateRevision;
}

void UActCharacterMovementComponent::StartFloating(float Duration)
{
	if (FloatingTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(FloatingTimerHandle);  // 清除当前的计时器
	}
	
	if (Duration != -1.0f)
	{
		FloatingDuration = Duration;
	}
	
	GravityScale = 0.0f;
	Velocity = FVector::ZeroVector;

	bIsFloating = true;
	
	if (FloatingDuration > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(FloatingTimerHandle, this, &ThisClass::EndFloating, FloatingDuration, false);
	}
}

void UActCharacterMovementComponent::StopFloating()
{
	if (!bIsFloating) return;
	
	GravityScale = 1;
	bIsFloating = false;
}

void UActCharacterMovementComponent::EndFloating()
{
	StopFloating();
}

void UActCharacterMovementComponent::ApplyAddMove(const float DeltaSeconds)
{
	if (!AddMoveState.bActive || !UpdatedComponent || DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	float StepSeconds = DeltaSeconds;
	if (AddMoveState.Duration > 0.0f)
	{
		const float Remaining = AddMoveState.Duration - AddMoveState.ElapsedTime;
		if (Remaining <= 0.0f)
		{
			ResetAddMoveState();
			return;
		}
		StepSeconds = FMath::Min(DeltaSeconds, Remaining);
	}

	AddMoveState.ElapsedTime += DeltaSeconds;

	const FVector Delta = AddMoveState.Velocity * StepSeconds;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		SlideAlongSurface(Delta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (AddMoveState.Duration > 0.0f && AddMoveState.ElapsedTime >= AddMoveState.Duration)
	{
		ResetAddMoveState();
	}
}

void UActCharacterMovementComponent::ResetAddMoveState()
{
	if (!AddMoveState.bActive)
	{
		return;
	}

	AddMoveState = FActAddMoveNetworkState();
	++AddMoveStateRevision;
	UpdateGravityOverride();
}

void UActCharacterMovementComponent::ResetAddRotationState()
{
	if (!AddRotationState.bActive)
	{
		return;
	}

	AddRotationState = FActAddRotationNetworkState();
	++AddMoveStateRevision;
}

bool UActCharacterMovementComponent::TryApplyAddRotation(const float DeltaSeconds)
{
	if (!AddRotationState.bActive || !UpdatedComponent)
	{
		return false;
	}

	const FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	if (AddRotationState.bUseDirection && AddRotationState.Direction.IsNearlyZero())
	{
		ResetAddRotationState();
		return false;
	}

	FRotator DesiredRotation = AddRotationState.bUseDirection ? AddRotationState.Direction.Rotation() : AddRotationState.Rotation;
	DesiredRotation.Pitch = 0.0f;
	DesiredRotation.Roll = 0.0f;

	if (AddRotationState.RotationRate < 0.0f)
	{
		MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, false);
		ResetAddRotationState();
		return true;
	}

	const FRotator DeltaRot = GetDeltaRotation(DeltaSeconds);
	FRotator NewRotation = CurrentRotation;
	NewRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);

	MoveUpdatedComponent(FVector::ZeroVector, NewRotation, false);

	const float RemainingYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(NewRotation.Yaw, DesiredRotation.Yaw));
	if (RemainingYaw <= AddRotationStopTolerance)
	{
		ResetAddRotationState();
	}

	return true;
}

void UActCharacterMovementComponent::UpdateGravityOverride()
{
	const bool bShouldOverride = AddMoveState.bActive && AddMoveState.bIgnoreGravity;
	if (bShouldOverride && !bGravityOverrideActive)
	{
		SavedGravityScale = GravityScale;
		GravityScale = 0.0f;
		Velocity.Z = 0.0f;
		bGravityOverrideActive = true;
		return;
	}

	if (!bShouldOverride && bGravityOverrideActive)
	{
		GravityScale = SavedGravityScale;
		bGravityOverrideActive = false;
	}
}

void UActCharacterMovementComponent::ApplyNetworkMoveData(const FActAddMoveNetworkState& InAddMoveState, const FActAddRotationNetworkState& InAddRotationState)
{
	AddMoveState = InAddMoveState;
	AddRotationState = InAddRotationState;
	UpdateGravityOverride();
}
