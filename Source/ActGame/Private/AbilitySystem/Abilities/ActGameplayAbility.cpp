// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/ActGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
#include "ActGameplayTags.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Character/ActCharacter.h"

namespace
{
double GetAbilityLogTimeSeconds(const FGameplayAbilityActorInfo* ActorInfo)
{
	const UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0;
}

FString GetAbilityOwnerName(const FGameplayAbilityActorInfo* ActorInfo)
{
	return GetNameSafe(ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr);
}
}

UActGameplayAbility::UActGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ClientOrServer;

	ActivationPolicy = EActAbilityActivationPolicy::OnInputTriggered;
	ActivationGroup = EActAbilityActivationGroup::Independent;
}

UActAbilitySystemComponent* UActGameplayAbility::GetActAbilitySystemComponentFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<UActAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr);
}

AActCharacter* UActGameplayAbility::GetActCharacterFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<AActCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}

void UActGameplayAbility::TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const
{
	// Try to activate if activation policy is on spawn.
	if (ActorInfo && !Spec.IsActive() && (ActivationPolicy == EActAbilityActivationPolicy::OnSpawn))
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		const AActor* AvatarActor = ActorInfo->AvatarActor.Get();

		// If avatar actor is torn off or about to die, don't try to activate until we get the new one.
		if (ASC && AvatarActor && !AvatarActor->GetTearOff() && (AvatarActor->GetLifeSpan() <= 0.0f))
		{
			const bool bIsLocalExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly);
			const bool bIsServerExecution = (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly) || (NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated);

			const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
			const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

			if (bClientShouldActivate || bServerShouldActivate)
			{
				ASC->TryActivateAbility(Spec.Handle);
			}
		}
	}
}

bool UActGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] CanActivate rejected. Ability=%s AbilityId=%s Owner=%s Reason=InvalidActorInfo Time=%.3f"),
			*GetNameSafe(this),
			*AbilityId.ToString(),
			*GetAbilityOwnerName(ActorInfo),
			GetAbilityLogTimeSeconds(ActorInfo));
		return false;
	}

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] CanActivate rejected. Ability=%s AbilityId=%s Owner=%s Reason=Super Time=%.3f RelevantTags=%s"),
			*GetNameSafe(this),
			*AbilityId.ToString(),
			*GetAbilityOwnerName(ActorInfo),
			GetAbilityLogTimeSeconds(ActorInfo),
			OptionalRelevantTags ? *OptionalRelevantTags->ToString() : TEXT("None"));
		return false;
	}

	//@TODO Possibly remove after setting up tag relationships
	UActAbilitySystemComponent* ActASC = CastChecked<UActAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (ActASC->IsActivationGroupBlocked(ActivationGroup))
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(ActGameplayTags::Ability_ActivateFail_ActivationGroup);
		}
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] CanActivate rejected. Ability=%s AbilityId=%s Owner=%s Reason=ActivationGroupBlocked Group=%d Time=%.3f"),
			*GetNameSafe(this),
			*AbilityId.ToString(),
			*GetAbilityOwnerName(ActorInfo),
			static_cast<int32>(ActivationGroup),
			GetAbilityLogTimeSeconds(ActorInfo));
		return false;
	}

	return true;
}

bool UActGameplayAbility::DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	// Specialized version to handle death exclusion and AbilityTags expansion via ASC

	bool bBlocked = false;
	bool bMissing = false;

	const UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
	const FGameplayTag& BlockedTag = AbilitySystemGlobals.ActivateFailTagsBlockedTag;
	const FGameplayTag& MissingTag = AbilitySystemGlobals.ActivateFailTagsMissingTag;

	// Check if any of this ability's tags are currently blocked
	if (AbilitySystemComponent.AreAbilityTagsBlocked(GetAssetTags()))
	{
		bBlocked = true;
	}

	const UActAbilitySystemComponent* ActASC = Cast<UActAbilitySystemComponent>(&AbilitySystemComponent);
	static FGameplayTagContainer AllRequiredTags;
	static FGameplayTagContainer AllBlockedTags;

	AllRequiredTags = ActivationRequiredTags;
	AllBlockedTags = ActivationBlockedTags;

	// Expand our ability tags to add additional required/blocked tags
	if (ActASC)
	{
		ActASC->GetAdditionalActivationTagRequirements(GetAssetTags(), AllRequiredTags, AllBlockedTags);
	}

	// Check to see the required/blocked tags for this ability
	if (AllBlockedTags.Num() || AllRequiredTags.Num())
	{
		static FGameplayTagContainer AbilitySystemComponentTags;
		
		AbilitySystemComponentTags.Reset();
		AbilitySystemComponent.GetOwnedGameplayTags(AbilitySystemComponentTags);

		if (AbilitySystemComponentTags.HasAny(AllBlockedTags))
		{
			if (OptionalRelevantTags && AbilitySystemComponentTags.HasTag(ActGameplayTags::Status_Death))
			{
				// If player is dead and was rejected due to blocking tags, give that feedback
				OptionalRelevantTags->AddTag(ActGameplayTags::Ability_ActivateFail_IsDead);
			}

			bBlocked = true;
		}

		if (!AbilitySystemComponentTags.HasAll(AllRequiredTags))
		{
			bMissing = true;
		}
	}

	if (SourceTags != nullptr)
	{
		if (SourceBlockedTags.Num() || SourceRequiredTags.Num())
		{
			if (SourceTags->HasAny(SourceBlockedTags))
			{
				bBlocked = true;
			}

			if (!SourceTags->HasAll(SourceRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (TargetTags != nullptr)
	{
		if (TargetBlockedTags.Num() || TargetRequiredTags.Num())
		{
			if (TargetTags->HasAny(TargetBlockedTags))
			{
				bBlocked = true;
			}

			if (!TargetTags->HasAll(TargetRequiredTags))
			{
				bMissing = true;
			}
		}
	}

	if (bBlocked)
	{
		if (OptionalRelevantTags && BlockedTag.IsValid())
		{
			OptionalRelevantTags->AddTag(BlockedTag);
		}
		return false;
	}
	if (bMissing)
	{
		if (OptionalRelevantTags && MissingTag.IsValid())
		{
			OptionalRelevantTags->AddTag(MissingTag);
		}
		return false;
	}

	return true;
}

void UActGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] Activate. Ability=%s AbilityId=%s Owner=%s Policy=%d NetPolicy=%d Local=%d Auth=%d PredKey=%d Time=%.3f Trigger=%s"),
		*GetNameSafe(this),
		*AbilityId.ToString(),
		*GetAbilityOwnerName(ActorInfo),
		static_cast<int32>(ActivationPolicy),
		static_cast<int32>(NetExecutionPolicy),
		ActorInfo ? ActorInfo->IsLocallyControlled() : 0,
		ActorInfo ? ActorInfo->IsNetAuthority() : 0,
		ActivationInfo.GetActivationPredictionKey().Current,
		GetAbilityLogTimeSeconds(ActorInfo),
		TriggerEventData ? *TriggerEventData->EventTag.ToString() : TEXT("None"));

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponentFromActorInfo())
	{
		// The active ability owns the chain context; windows register against this context.
		ActASC->BeginAbilityChain(*this, Handle);
	}
}

void UActGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility)
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] Cancel. Ability=%s AbilityId=%s Owner=%s Local=%d Auth=%d PredKey=%d Replicate=%d Time=%.3f"),
		*GetNameSafe(this),
		*AbilityId.ToString(),
		*GetAbilityOwnerName(ActorInfo),
		ActorInfo ? ActorInfo->IsLocallyControlled() : 0,
		ActorInfo ? ActorInfo->IsNetAuthority() : 0,
		ActivationInfo.GetActivationPredictionKey().Current,
		bReplicateCancelAbility,
		GetAbilityLogTimeSeconds(ActorInfo));

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

void UActGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleAbility] End. Ability=%s AbilityId=%s Owner=%s Local=%d Auth=%d PredKey=%d Replicate=%d Cancelled=%d Time=%.3f"),
		*GetNameSafe(this),
		*AbilityId.ToString(),
		*GetAbilityOwnerName(ActorInfo),
		ActorInfo ? ActorInfo->IsLocallyControlled() : 0,
		ActorInfo ? ActorInfo->IsNetAuthority() : 0,
		ActivationInfo.GetActivationPredictionKey().Current,
		bReplicateEndAbility,
		bWasCancelled,
		GetAbilityLogTimeSeconds(ActorInfo));

	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponentFromActorInfo())
	{
		// Release the chain context if this ability is the owner.
		ActASC->EndAbilityChain(*this, Handle);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UActGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	TryActivateAbilityOnSpawn(ActorInfo, Spec);
}



