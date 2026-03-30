// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/Tasks/AT_PlayAddMoveMontageAndWaitForEvent.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Animation/AnimMontage.h"
#include "Character/ActCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AT_PlayAddMoveMontageAndWaitForEvent)

namespace
{
bool CanApplyLocalAddMoveMontage(const ACharacter* Character, const UGameplayAbility* Ability)
{
	if (!Character || !Ability)
	{
		return false;
	}

	if (Character->GetLocalRole() == ROLE_Authority)
	{
		return true;
	}

	if (Character->GetLocalRole() != ROLE_AutonomousProxy)
	{
		return false;
	}

	switch (Ability->GetNetExecutionPolicy())
	{
	case EGameplayAbilityNetExecutionPolicy::LocalPredicted:
	case EGameplayAbilityNetExecutionPolicy::ServerInitiated:
		return true;
	default:
		return false;
	}
}

void DisableNativeRootMotionIfNeeded(UAnimInstance* AnimInstance, UAnimMontage* Montage, bool& bOutPushedDisableRootMotion)
{
	if (bOutPushedDisableRootMotion || !AnimInstance || !Montage)
	{
		return;
	}

	if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(Montage))
	{
		MontageInstance->PushDisableRootMotion();
		bOutPushedDisableRootMotion = true;
	}
}

bool ResolveMontageSectionTrackRange(const UAnimMontage* Montage, const int32 SectionIndex, float& OutStartTrackPosition, float& OutEndTrackPosition)
{
	if (!Montage || SectionIndex == INDEX_NONE || !Montage->CompositeSections.IsValidIndex(SectionIndex))
	{
		return false;
	}

	const FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
	const float MontageLength = Montage->GetPlayLength();
	const float SectionStartTrackPosition = FMath::Clamp(Section.GetTime(), 0.0f, MontageLength);

	float SectionEndTrackPosition = MontageLength;
	for (int32 NextSectionIndex = SectionIndex + 1; NextSectionIndex < Montage->CompositeSections.Num(); ++NextSectionIndex)
	{
		const float CandidateStartTrackPosition = FMath::Clamp(Montage->CompositeSections[NextSectionIndex].GetTime(), 0.0f, MontageLength);
		if (CandidateStartTrackPosition > SectionStartTrackPosition + UE_SMALL_NUMBER)
		{
			SectionEndTrackPosition = CandidateStartTrackPosition;
			break;
		}
	}

	if (SectionEndTrackPosition <= SectionStartTrackPosition + UE_SMALL_NUMBER)
	{
		return false;
	}

	OutStartTrackPosition = SectionStartTrackPosition;
	OutEndTrackPosition = SectionEndTrackPosition;
	return true;
}

bool ResolveMontageAddMoveTrackRange(
	const UAnimMontage* Montage,
	const FName StartSectionName,
	const EActAddMoveMontageRangeMode RangeMode,
	const FName EndSectionName,
	float& OutStartTrackPosition,
	float& OutEndTrackPosition)
{
	if (!Montage)
	{
		return false;
	}

	const float MontageLength = Montage->GetPlayLength();
	float StartTrackPosition = 0.0f;
	float EndTrackPosition = MontageLength;

	const int32 StartSectionIndex = StartSectionName.IsNone() ? INDEX_NONE : Montage->GetSectionIndex(StartSectionName);
	if (StartSectionIndex != INDEX_NONE)
	{
		if (!ResolveMontageSectionTrackRange(Montage, StartSectionIndex, StartTrackPosition, EndTrackPosition))
		{
			return false;
		}
	}

	switch (RangeMode)
	{
	case EActAddMoveMontageRangeMode::CurrentSection:
		break;
	case EActAddMoveMontageRangeMode::ThroughSection:
		if (!EndSectionName.IsNone())
		{
			const int32 EndSectionIndex = Montage->GetSectionIndex(EndSectionName);
			if (EndSectionIndex != INDEX_NONE)
			{
				float SectionStart = 0.0f;
				float SectionEnd = 0.0f;
				if (ResolveMontageSectionTrackRange(Montage, EndSectionIndex, SectionStart, SectionEnd))
				{
					EndTrackPosition = SectionEnd;
				}
			}
		}
		break;
	case EActAddMoveMontageRangeMode::EntireMontage:
		if (StartSectionIndex == INDEX_NONE)
		{
			StartTrackPosition = 0.0f;
		}
		EndTrackPosition = MontageLength;
		break;
	}

	if (FMath::IsNearlyEqual(StartTrackPosition, EndTrackPosition, UE_SMALL_NUMBER))
	{
		return false;
	}

	OutStartTrackPosition = FMath::Clamp(StartTrackPosition, 0.0f, MontageLength);
	OutEndTrackPosition = FMath::Clamp(EndTrackPosition, 0.0f, MontageLength);
	return OutEndTrackPosition > OutStartTrackPosition + UE_SMALL_NUMBER;
}

FName ResolveInitialAddMoveSection(const UAnimInstance* AnimInstance, const UAnimMontage* Montage, const FName RequestedStartSection, const EActAddMoveMontageRangeMode RangeMode)
{
	if (!RequestedStartSection.IsNone() || RangeMode != EActAddMoveMontageRangeMode::CurrentSection)
	{
		return RequestedStartSection;
	}

	return (AnimInstance && Montage) ? AnimInstance->Montage_GetCurrentSection(Montage) : NAME_None;
}
}

UAT_PlayAddMoveMontageAndWaitForEvent::UAT_PlayAddMoveMontageAndWaitForEvent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAT_PlayAddMoveMontageAndWaitForEvent* UAT_PlayAddMoveMontageAndWaitForEvent::CreatePlayAddMoveMontageAndWaitForEvent(
	UGameplayAbility* OwningAbility,
	FName TaskInstanceName,
	UAnimMontage* InMontageToPlay,
	FGameplayTagContainer InEventTags,
	float InRate,
	FName InStartSection,
	bool bInStopWhenAbilityEnds,
	FActAddMoveMontageSettings InAddMoveSettings)
{
	UAbilitySystemGlobals::NonShipping_ApplyGlobalAbilityScaler_Rate(InRate);

	UAT_PlayAddMoveMontageAndWaitForEvent* NewTask = NewAbilityTask<UAT_PlayAddMoveMontageAndWaitForEvent>(OwningAbility, TaskInstanceName);
	NewTask->MontageToPlay = InMontageToPlay;
	NewTask->EventTags = InEventTags;
	NewTask->Rate = InRate;
	NewTask->StartSection = InStartSection;
	NewTask->bStopWhenAbilityEnds = bInStopWhenAbilityEnds;
	NewTask->AddMoveSettings = InAddMoveSettings;
	return NewTask;
}

void UAT_PlayAddMoveMontageAndWaitForEvent::Activate()
{
	if (!Ability)
	{
		return;
	}

	bool bPlayedMontage = false;
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		if (UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr)
		{
			EventHandle = ActASC->AddGameplayEventTagContainerDelegate(EventTags, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::OnGameplayEvent));

			if (ActASC->PlayMontage(Ability, Ability->GetCurrentActivationInfo(), MontageToPlay, Rate, StartSection) > 0.0f)
			{
				if (!ShouldBroadcastAbilityTaskDelegates())
				{
					return;
				}

				CancelledHandle = Ability->OnGameplayAbilityCancelled.AddUObject(this, &ThisClass::OnAbilityCancelled);

				BlendingOutDelegate.BindUObject(this, &ThisClass::OnMontageBlendingOut);
				AnimInstance->Montage_SetBlendingOutDelegate(BlendingOutDelegate, MontageToPlay);

				MontageEndedDelegate.BindUObject(this, &ThisClass::OnMontageEnded);
				AnimInstance->Montage_SetEndDelegate(MontageEndedDelegate, MontageToPlay);

				if (const ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
				{
					if (CanApplyLocalAddMoveMontage(Character, Ability))
					{
						if (AddMoveSettings.UsesExtractedRootMotion() && MontageToPlay && Rate > UE_SMALL_NUMBER)
						{
							const FName InitialAddMoveSection = ResolveInitialAddMoveSection(AnimInstance, MontageToPlay, StartSection, AddMoveSettings.RangeMode);
							RefreshPredictedAddMoveForSection(InitialAddMoveSection);
						}
						else if (AddMoveSettings.UsesProceduralPresentation())
						{
							DisableNativeRootMotionIfNeeded(AnimInstance, MontageToPlay, bPushedDisableRootMotion);
						}
					}
				}

				bPlayedMontage = true;
			}
		}
		else
		{
			UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayAddMoveMontageAndWaitForEvent call to PlayMontage failed!"));
		}
	}
	else
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayAddMoveMontageAndWaitForEvent called on invalid AbilitySystemComponent"));
	}

	if (!bPlayedMontage)
	{
		UE_LOG(LogActAbilitySystem, Warning, TEXT("UAT_PlayAddMoveMontageAndWaitForEvent called in Ability %s failed to play montage %s; Task Instance Name %s."),
			*GetNameSafe(Ability),
			*GetNameSafe(MontageToPlay),
			*InstanceName.ToString());
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}

	SetWaitingOnAvatar();
}

void UAT_PlayAddMoveMontageAndWaitForEvent::TickTask(const float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!AddMoveSettings.UsesExtractedRootMotion() || !MontageToPlay || Rate <= UE_SMALL_NUMBER)
	{
		return;
	}

	const FName CurrentSectionName = GetCurrentMontageSectionName();
	if (CurrentSectionName.IsNone() || CurrentSectionName == LastAppliedAddMoveSection)
	{
		return;
	}

	RefreshPredictedAddMoveForSection(CurrentSectionName);
}

void UAT_PlayAddMoveMontageAndWaitForEvent::ExternalCancel()
{
	check(AbilitySystemComponent.Get());

	OnAbilityCancelled();
	Super::ExternalCancel();
}

FString UAT_PlayAddMoveMontageAndWaitForEvent::GetDebugString() const
{
	UAnimMontage* PlayingMontage = nullptr;
	if (Ability)
	{
		const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
		if (UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr)
		{
			PlayingMontage = AnimInstance->Montage_IsActive(MontageToPlay) ? MontageToPlay : AnimInstance->GetCurrentActiveMontage();
		}
	}

	return FString::Printf(TEXT("PlayAddMoveMontageAndWaitForEvent. MontageToPlay: %s  (Currently Playing): %s"),
		*GetNameSafe(MontageToPlay),
		*GetNameSafe(PlayingMontage));
}

void UAT_PlayAddMoveMontageAndWaitForEvent::OnDestroy(const bool bInOwnerFinished)
{
	if (Ability)
	{
		Ability->OnGameplayAbilityCancelled.Remove(CancelledHandle);
		if (bInOwnerFinished && bStopWhenAbilityEnds)
		{
			StopPlayingMontage();
		}
	}

	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->RemoveGameplayEventTagContainerDelegate(EventTags, EventHandle);
	}

	if (ACharacter* Character = Cast<ACharacter>(GetAvatarActor()))
	{
		if (AddMoveHandle != INDEX_NONE)
		{
			if (UActCharacterMovementComponent* ActMovementComponent = Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement()))
			{
				ActMovementComponent->StopMotion(AddMoveHandle);
			}
			AddMoveHandle = INDEX_NONE;
		}

		if (bPushedDisableRootMotion && Ability)
		{
			if (const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo())
			{
				if (UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance())
				{
					if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
					{
						MontageInstance->PopDisableRootMotion();
					}
				}
			}

			bPushedDisableRootMotion = false;
		}
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UAT_PlayAddMoveMontageAndWaitForEvent::StopPlayingMontage()
{
	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	if (!ActorInfo)
	{
		return false;
	}

	UAnimInstance* AnimInstance = ActorInfo->GetAnimInstance();
	if (!AnimInstance)
	{
		return false;
	}

	if (AbilitySystemComponent.IsValid() && Ability)
	{
		if (AbilitySystemComponent->GetAnimatingAbility() == Ability && AbilitySystemComponent->GetCurrentMontage() == MontageToPlay)
		{
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
			{
				MontageInstance->OnMontageBlendingOutStarted.Unbind();
				MontageInstance->OnMontageEnded.Unbind();
			}

			AbilitySystemComponent->CurrentMontageStop();
			return true;
		}
	}

	return false;
}

bool UAT_PlayAddMoveMontageAndWaitForEvent::RefreshPredictedAddMoveForSection(const FName SectionName)
{
	ACharacter* Character = Cast<ACharacter>(GetAvatarActor());
	if (!Character || !MontageToPlay)
	{
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability ? Ability->GetCurrentActorInfo() : nullptr;
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	if (!bPushedDisableRootMotion)
	{
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(MontageToPlay))
		{
			MontageInstance->PushDisableRootMotion();
			bPushedDisableRootMotion = true;
		}
	}

	UActCharacterMovementComponent* AddMoveComponent = Cast<UActCharacterMovementComponent>(Character->GetCharacterMovement());
	if (!AddMoveComponent)
	{
		return false;
	}

	float MotionStartTrackPosition = 0.0f;
	float MotionEndTrackPosition = 0.0f;
	if (!ResolveMontageAddMoveTrackRange(MontageToPlay, SectionName, AddMoveSettings.RangeMode, AddMoveSettings.EndSectionName, MotionStartTrackPosition, MotionEndTrackPosition))
	{
		if (AddMoveHandle != INDEX_NONE)
		{
			AddMoveComponent->StopMotion(AddMoveHandle);
			AddMoveHandle = INDEX_NONE;
		}

		LastAppliedAddMoveSection = SectionName;
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] ExtractedAddMove skipped. Ability=%s Montage=%s StartSection=%s"),
			*GetNameSafe(Ability),
			*GetNameSafe(MontageToPlay),
			*SectionName.ToString());
		return false;
	}

	const float ExtractedDuration = FMath::Abs(MotionEndTrackPosition - MotionStartTrackPosition) / FMath::Max(FMath::Abs(Rate), UE_SMALL_NUMBER);
	if (ExtractedDuration <= UE_SMALL_NUMBER)
	{
		LastAppliedAddMoveSection = SectionName;
		return false;
	}

	if (AddMoveHandle == INDEX_NONE && AddMoveSettings.bBrakeMovementOnStart)
	{
		AddMoveComponent->StopMovementImmediately();
	}

	AddMoveHandle = AddMoveComponent->ApplyRootMotionMotion(
		MontageToPlay,
		MotionStartTrackPosition,
		MotionEndTrackPosition,
		ExtractedDuration,
		AddMoveSettings.BasisMode,
		AddMoveSettings.bApplyRotation,
		AddMoveSettings.bRespectAddMoveRotation,
		AddMoveSettings.bIgnoreZAccumulate,
		FActMotionRotationParams(),
		EActMotionProvenance::OwnerPredicted,
		AddMoveHandle,
		GetOrCreateAddMoveSyncId());

	LastAppliedAddMoveSection = SectionName;
	return AddMoveHandle != INDEX_NONE;
}

FName UAT_PlayAddMoveMontageAndWaitForEvent::GetCurrentMontageSectionName() const
{
	if (!Ability || !MontageToPlay)
	{
		return NAME_None;
	}

	const FGameplayAbilityActorInfo* ActorInfo = Ability->GetCurrentActorInfo();
	UAnimInstance* AnimInstance = ActorInfo ? ActorInfo->GetAnimInstance() : nullptr;
	return AnimInstance ? AnimInstance->Montage_GetCurrentSection(MontageToPlay) : NAME_None;
}

int32 UAT_PlayAddMoveMontageAndWaitForEvent::GetOrCreateAddMoveSyncId() const
{
	if (AddMoveSyncId != INDEX_NONE)
	{
		return AddMoveSyncId;
	}

	uint32 Hash = GetTypeHash(GetNameSafe(MontageToPlay));
	Hash = HashCombine(Hash, GetTypeHash(InstanceName));

	if (Ability)
	{
		Hash = HashCombine(Hash, FCrc::StrCrc32(*Ability->GetCurrentAbilitySpecHandle().ToString()));
		Hash = HashCombine(Hash, ::GetTypeHash(Ability->GetCurrentActivationInfo().GetActivationPredictionKey().Current));
	}

	AddMoveSyncId = static_cast<int32>(Hash & 0x7fffffff);
	if (AddMoveSyncId == INDEX_NONE)
	{
		AddMoveSyncId = FCrc::StrCrc32(TEXT("AddMoveMontageSync"));
	}

	return AddMoveSyncId;
}

UActAbilitySystemComponent* UAT_PlayAddMoveMontageAndWaitForEvent::GetActAbilitySystemComponent()
{
	return Cast<UActAbilitySystemComponent>(AbilitySystemComponent);
}

void UAT_PlayAddMoveMontageAndWaitForEvent::OnMontageBlendingOut(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Ability && Ability->GetCurrentMontage() == MontageToPlay && Montage == MontageToPlay)
	{
		AbilitySystemComponent->ClearAnimatingAbility(Ability);
	}

	if (bInterrupted)
	{
		if (ShouldBroadcastAbilityTaskDelegates())
		{
			OnInterrupted.Broadcast(FGameplayTag(), FGameplayEventData());
		}
	}
	else if (ShouldBroadcastAbilityTaskDelegates())
	{
		OnBlendOut.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UAT_PlayAddMoveMontageAndWaitForEvent::OnAbilityCancelled()
{
	if (StopPlayingMontage() && ShouldBroadcastAbilityTaskDelegates())
	{
		OnCancelled.Broadcast(FGameplayTag(), FGameplayEventData());
	}
}

void UAT_PlayAddMoveMontageAndWaitForEvent::OnMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (!bInterrupted && ShouldBroadcastAbilityTaskDelegates())
	{
		OnCompleted.Broadcast(FGameplayTag(), FGameplayEventData());
	}

	EndTask();
}

void UAT_PlayAddMoveMontageAndWaitForEvent::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (ShouldBroadcastAbilityTaskDelegates() && Payload)
	{
		FGameplayEventData TempData = *Payload;
		TempData.EventTag = EventTag;
		EventReceived.Broadcast(EventTag, TempData);
	}
}
