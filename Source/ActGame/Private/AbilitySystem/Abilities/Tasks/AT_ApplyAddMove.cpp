// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Tasks/AT_ApplyAddMove.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/Crc.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AT_ApplyAddMove)

UAT_ApplyAddMove::UAT_ApplyAddMove(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAT_ApplyAddMove* UAT_ApplyAddMove::ApplyAddMove(
	UGameplayAbility* OwningAbility,
	const FName TaskInstanceName,
	const EActAddMoveSpace InSpace,
	FVector InDirection,
	const float InStrength,
	float InDuration,
	UCurveFloat* InStrengthOverTime,
	USkeletalMeshComponent* InMesh)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Duration(InDuration);

	UAT_ApplyAddMove* Task = NewAbilityTask<UAT_ApplyAddMove>(OwningAbility, TaskInstanceName);
	Task->Space = InSpace;
	Task->Direction = InDirection.GetSafeNormal();
	Task->Strength = InStrength;
	Task->Duration = InDuration;
	Task->StrengthOverTime = InStrengthOverTime;
	Task->Mesh = InMesh;
	return Task;
}

void UAT_ApplyAddMove::Activate()
{
	if (!ApplyAuthoredAddMove())
	{
		EndTask();
		return;
	}

	if (!HasInfiniteDuration())
	{
		if (UWorld* World = GetWorld())
		{
			if (Duration <= 0.0f)
			{
				World->GetTimerManager().SetTimerForNextTick(this, &UAT_ApplyAddMove::OnTimeFinish);
			}
			else
			{
				World->GetTimerManager().SetTimer(FinishTimerHandle, this, &UAT_ApplyAddMove::OnTimeFinish, Duration, false);
			}
		}
	}

	SetWaitingOnAvatar();
}

void UAT_ApplyAddMove::ExternalCancel()
{
	Super::ExternalCancel();
	EndTask();
}

void UAT_ApplyAddMove::OnDestroy(const bool bInOwnerFinished)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FinishTimerHandle);
	}

	if (AddMoveHandle != INDEX_NONE)
	{
		if (UActCharacterMovementComponent* MovementComponent = GetActMovementComponent())
		{
			MovementComponent->StopAddMove(AddMoveHandle);
		}

		AddMoveHandle = INDEX_NONE;
	}

	Super::OnDestroy(bInOwnerFinished);
}

FString UAT_ApplyAddMove::GetDebugString() const
{
	return FString::Printf(
		TEXT("ApplyAddMove. Space=%d Direction=%s Strength=%.2f Duration=%.2f Handle=%d"),
		static_cast<int32>(Space),
		*Direction.ToString(),
		Strength,
		Duration,
		AddMoveHandle);
}

UActCharacterMovementComponent* UAT_ApplyAddMove::GetActMovementComponent() const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	return Character ? Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()) : nullptr;
}

USkeletalMeshComponent* UAT_ApplyAddMove::ResolveMeshBasis() const
{
	if (Mesh)
	{
		return Mesh;
	}

	const ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	return Character ? Character->GetMesh() : nullptr;
}

bool UAT_ApplyAddMove::ApplyAuthoredAddMove()
{
	UActCharacterMovementComponent* MovementComponent = GetActMovementComponent();
	if (!MovementComponent)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove called in Ability %s with null ActCharacterMovementComponent; Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	if (Direction.IsNearlyZero() || FMath::IsNearlyZero(Strength) || FMath::IsNearlyZero(Duration))
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove called in Ability %s with invalid movement params. Direction=%s Strength=%.3f Duration=%.3f Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*Direction.ToString(),
			Strength,
			Duration,
			*InstanceName.ToString());
		return false;
	}

	const FVector Velocity = Direction * Strength;
	const EActAddMoveCurveType CurveType = StrengthOverTime ? EActAddMoveCurveType::CustomCurve : EActAddMoveCurveType::Constant;
	const int32 SyncId = ShouldUseNetworkSyncId() ? GetOrCreateAddMoveSyncId() : INDEX_NONE;

	switch (Space)
	{
	case EActAddMoveSpace::World:
		AddMoveHandle = MovementComponent->SetAddMoveWorld(Velocity, Duration, CurveType, StrengthOverTime, INDEX_NONE, SyncId);
		break;
	case EActAddMoveSpace::Actor:
		AddMoveHandle = MovementComponent->SetAddMove(Velocity, Duration, CurveType, StrengthOverTime, INDEX_NONE, SyncId);
		break;
	case EActAddMoveSpace::Mesh:
		AddMoveHandle = MovementComponent->SetAddMoveWithMesh(ResolveMeshBasis(), Velocity, Duration, CurveType, StrengthOverTime, INDEX_NONE, SyncId);
		break;
	default:
		AddMoveHandle = INDEX_NONE;
		break;
	}

	if (AddMoveHandle == INDEX_NONE)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_ApplyAddMove failed to allocate movement handle. Ability=%s Task Instance Name %s."),
			Ability ? *Ability->GetName() : TEXT("NULL"),
			*InstanceName.ToString());
		return false;
	}

	return true;
}

bool UAT_ApplyAddMove::HasInfiniteDuration() const
{
	return Duration < 0.0f;
}

void UAT_ApplyAddMove::OnTimeFinish()
{
	if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnFinish.Broadcast();
	}

	EndTask();
}

bool UAT_ApplyAddMove::ShouldUseNetworkSyncId() const
{
	if (!Ability)
	{
		return false;
	}

	return Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

int32 UAT_ApplyAddMove::GetOrCreateAddMoveSyncId() const
{
	if (AddMoveSyncId != INDEX_NONE)
	{
		return AddMoveSyncId;
	}

	uint32 Hash = GetTypeHash(InstanceName);
	if (Ability)
	{
		Hash = HashCombine(Hash, GetTypeHash(Ability->GetClass()));
		Hash = HashCombine(Hash, FCrc::StrCrc32(*Ability->GetCurrentAbilitySpecHandle().ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(Ability->GetCurrentActivationInfo().GetActivationPredictionKey().Current));
	}

	AddMoveSyncId = static_cast<int32>(Hash & 0x7fffffff);
	if (AddMoveSyncId == INDEX_NONE)
	{
		AddMoveSyncId = FCrc::StrCrc32(TEXT("ApplyAddMoveSync"));
	}

	return AddMoveSyncId;
}
