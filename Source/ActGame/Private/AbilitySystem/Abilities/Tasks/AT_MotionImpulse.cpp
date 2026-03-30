#include "AbilitySystem/Abilities/Tasks/AT_MotionImpulse.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AT_MotionImpulse)

UAT_MotionImpulse::UAT_MotionImpulse(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAT_MotionImpulse* UAT_MotionImpulse::MotionImpulse(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const EActMotionBasisMode InBasisMode,
	FVector InDirection,
	const float InStrength,
	float InDuration,
	const EActMotionRotationSourceType InRotationSourceType,
	AActor* InRotationTarget,
	const EActMotionRotationActorMode InRotationActorMode,
	UCurveFloat* InStrengthOverTime,
	const bool bInUseOppositeMovementDirection,
	const bool bInAdditiveRotation,
	const EActMotionRotationPriority InRotationPriority)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(InDuration);

	UAT_MotionImpulse* Task = NewAbilityTask<UAT_MotionImpulse>(OwningAbility, TaskInstanceName);
	Task->BasisMode = InBasisMode;
	Task->Direction = InDirection.GetSafeNormal();
	Task->Strength = InStrength;
	Task->Duration = InDuration;
	Task->RotationSourceType = InRotationSourceType;
	Task->RotationTarget = InRotationTarget;
	Task->RotationActorMode = InRotationActorMode;
	Task->StrengthOverTime = InStrengthOverTime;
	Task->bUseOppositeMovementDirection = bInUseOppositeMovementDirection;
	Task->bAdditiveRotation = bInAdditiveRotation;
	Task->RotationPriority = InRotationPriority;
	return Task;
}

void UAT_MotionImpulse::Activate()
{
	if (!ApplyImpulseMotion())
	{
		EndTask();
		return;
	}

	if (MotionHandle == INDEX_NONE)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimerForNextTick(this, &UAT_MotionImpulse::OnTimeFinish);
		}
		else
		{
			OnTimeFinish();
		}
		return;
	}

	if (!bFinishWhenMotionCompletes && !HasInfiniteDuration())
	{
		if (UWorld* World = GetWorld())
		{
			if (Duration <= 0.0f)
			{
				World->GetTimerManager().SetTimerForNextTick(this, &UAT_MotionImpulse::OnTimeFinish);
			}
			else
			{
				World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UAT_MotionImpulse::OnTimeFinish, Duration, false);
			}
		}
	}

	SetWaitingOnAvatar();
}

void UAT_MotionImpulse::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!bFinishWhenMotionCompletes || MotionSyncId == INDEX_NONE)
	{
		return;
	}

	if (UActCharacterMovementComponent* MovementComponent = GetActMovementComponent())
	{
		if (MovementComponent->HasMotionBySyncId(MotionSyncId))
		{
			return;
		}
	}

	MotionHandle = INDEX_NONE;
	MotionSyncId = INDEX_NONE;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}

	EndTask();
}

void UAT_MotionImpulse::ExternalCancel()
{
	Super::ExternalCancel();
	EndTask();
}

void UAT_MotionImpulse::OnDestroy(const bool bInOwnerFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	if (MotionHandle != INDEX_NONE)
	{
		if (UActCharacterMovementComponent* MovementComponent = GetActMovementComponent())
		{
			MovementComponent->StopMotion(MotionHandle);
		}

		MotionHandle = INDEX_NONE;
	}

	Super::OnDestroy(bInOwnerFinished);
}

FString UAT_MotionImpulse::GetDebugString() const
{
	return FString::Printf(
		TEXT("MotionImpulse. BasisMode=%d Direction=%s Strength=%.2f Duration=%.2f RotationSourceType=%d Handle=%d"),
		static_cast<int32>(BasisMode),
		*Direction.ToString(),
		Strength,
		Duration,
		static_cast<int32>(RotationSourceType),
		MotionHandle);
}

UActCharacterMovementComponent* UAT_MotionImpulse::GetActMovementComponent() const
{
	return UActCharacterMovementComponent::ResolveActMovementComponent(GetAvatarActor());
}

bool UAT_MotionImpulse::ApplyImpulseMotion()
{
	UActCharacterMovementComponent* MovementComponent = GetActMovementComponent();
	if (!MovementComponent)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_MotionImpulse called in Ability %s with null ActCharacterMovementComponent; Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	AActor* AvatarActor = GetAvatarActor();
	if (!AvatarActor || !AvatarActor->HasAuthority())
	{
		// Server-initiated hit react abilities also execute on remote clients for presentation.
		// MotionImpulse is authoritative-only, so non-authority instances should quietly no-op.
		MotionHandle = INDEX_NONE;
		MotionSyncId = INDEX_NONE;
		return true;
	}

	if (Direction.IsNearlyZero() || FMath::IsNearlyZero(Strength) || FMath::IsNearlyZero(Duration))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_MotionImpulse called in Ability %s with invalid impulse params. Direction=%s Strength=%.3f Duration=%.3f Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*Direction.ToString(),
			Strength,
			Duration,
			*InstanceName.ToString());
		return false;
	}

	const FVector AuthoredVelocity = Direction * Strength;
	const FVector HorizontalVelocity(AuthoredVelocity.X, AuthoredVelocity.Y, 0.0f);
	const float VerticalVelocity = AuthoredVelocity.Z;
	const bool bHasHorizontalMotion = !HorizontalVelocity.IsNearlyZero();
	const bool bHasVerticalLaunch = !FMath::IsNearlyZero(VerticalVelocity);
	const FActMotionRotationParams RotationParams = BuildRotationParams();
	const bool bHasRotationMotion = RotationParams.IsValidRequest();
	bFinishWhenMotionCompletes = bHasRotationMotion;

	if (!bHasHorizontalMotion && !bHasVerticalLaunch && !bHasRotationMotion)
	{
		return false;
	}

	const EActMotionCurveType CurveType = StrengthOverTime ? EActMotionCurveType::CustomCurve : EActMotionCurveType::Constant;
	MotionSyncId = MovementComponent->GenerateMotionSyncId();

	FActMotionParams Params;
	Params.SourceType = EActMotionSourceType::Velocity;
	Params.BasisMode = BasisMode;
	Params.Velocity = HorizontalVelocity;
	Params.LaunchVelocity = FVector(0.0f, 0.0f, VerticalVelocity);
	Params.bOverrideLaunchXY = false;
	Params.bOverrideLaunchZ = true;
	Params.Duration = Duration;
	Params.CurveType = CurveType;
	Params.Curve = StrengthOverTime;
	Params.Rotation = RotationParams;
	Params.Provenance = EActMotionProvenance::AuthorityExternal;
	Params.SyncId = MotionSyncId;

	MotionHandle = MovementComponent->ApplyMotion(Params, nullptr, INDEX_NONE);
	if (MotionHandle == INDEX_NONE)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_MotionImpulse failed to allocate movement handle. Ability=%s Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	return true;
}

bool UAT_MotionImpulse::HasInfiniteDuration() const
{
	return Duration < 0.0f;
}

void UAT_MotionImpulse::OnTimeFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}

	EndTask();
}

FActMotionRotationParams UAT_MotionImpulse::BuildRotationParams() const
{
	FActMotionRotationParams RotationParams;
	RotationParams.bClearOnReached = true;
	RotationParams.bIsAdditive = bAdditiveRotation;
	RotationParams.Priority = RotationPriority;

	EActMotionRotationSourceType EffectiveRotationSourceType = RotationSourceType;
	if (EffectiveRotationSourceType == EActMotionRotationSourceType::None)
	{
		if (IsValid(RotationTarget))
		{
			EffectiveRotationSourceType = EActMotionRotationSourceType::Actor;
		}
	}

	switch (EffectiveRotationSourceType)
	{
	case EActMotionRotationSourceType::Direction:
		{
			FVector RotationDirection = bUseOppositeMovementDirection ? -Direction : Direction;
			RotationDirection.Z = 0.0f;
			RotationParams.SourceType = EActMotionRotationSourceType::Direction;
			RotationParams.Direction = RotationDirection.GetSafeNormal();
			RotationParams.bFreezeDirectionAtStart = true;
		}
		break;
	case EActMotionRotationSourceType::Actor:
		RotationParams.SourceType = EActMotionRotationSourceType::Actor;
		RotationParams.TargetActor = RotationTarget;
		RotationParams.ActorMode = RotationActorMode;
		RotationParams.bFreezeDirectionAtStart =
			bAdditiveRotation ||
			RotationActorMode == EActMotionRotationActorMode::FaceTarget ||
			RotationActorMode == EActMotionRotationActorMode::BackToTarget;
		break;
	case EActMotionRotationSourceType::None:
	default:
		break;
	}

	return RotationParams;
}
