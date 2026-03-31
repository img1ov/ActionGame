#include "Character/ActCharacterMovementComponent.h"

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
	bIsOnGround = IsMovingOnGround();

	DefaultMovementStateParams.MaxWalkSpeed = MaxWalkSpeed;
	DefaultMovementStateParams.MaxAcceleration = MaxAcceleration;
	DefaultMovementStateParams.BrakingDecelerationWalking = BrakingDecelerationWalking;
	DefaultMovementStateParams.GroundFriction = GroundFriction;
	DefaultMovementStateParams.RotationRate = RotationRate;
	DefaultMovementStateParams.bOrientRotationToMovement = bOrientRotationToMovement;
	DefaultMovementStateParams.bUseControllerDesiredRotation = bUseControllerDesiredRotation;
}

UActCharacterMovementComponent* UActCharacterMovementComponent::ResolveActMovementComponent(const AActor* AvatarActor)
{
	const ACharacter* Character = Cast<ACharacter>(AvatarActor);
	return Character ? Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
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

bool UActCharacterMovementComponent::CanAttemptJump() const
{
	return IsJumpAllowed() && (IsMovingOnGround() || IsFalling());
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
