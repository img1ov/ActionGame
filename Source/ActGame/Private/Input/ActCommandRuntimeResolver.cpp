// Copyright

#include "Input/ActCommandRuntimeResolver.h"

#include "AbilitySystem/Abilities/ActGameplayAbility.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"

void FActCommandRuntimeResolver::Reset()
{
	ClearAbilityChainWindows();
	ClearCachedAbility();
}

void FActCommandRuntimeResolver::ClearAbilityChainWindows()
{
	ActiveChainWindows.Reset();
	ActiveChainByCommand.Reset();
}

void FActCommandRuntimeResolver::ClearCachedAbility()
{
	CachedAbilityHandle = FGameplayAbilitySpecHandle();
}

void FActCommandRuntimeResolver::RegisterAbilityChainWindow(
	const FName WindowId,
	const TArray<FActAbilityChainEntry>& ChainEntries)
{
	if (WindowId.IsNone())
	{
		return;
	}

	UnregisterAbilityChainWindow(WindowId);

	TArray<FActAbilityChainEntry> ValidEntries;
	ValidEntries.Reserve(ChainEntries.Num());

	for (const FActAbilityChainEntry& Entry : ChainEntries)
	{
		if (!Entry.CommandTag.IsValid() || Entry.AbilityID.IsNone())
		{
			continue;
		}

		ValidEntries.Add(Entry);
	}

	if (ValidEntries.IsEmpty())
	{
		return;
	}

	ActiveChainWindows.Add(WindowId, ValidEntries);

	for (const FActAbilityChainEntry& Entry : ValidEntries)
	{
		FActAbilityChainRuntimeEntry RuntimeEntry;
		RuntimeEntry.CommandTag = Entry.CommandTag;
		RuntimeEntry.AbilityID = Entry.AbilityID;
		RuntimeEntry.WindowId = WindowId;
		ActiveChainByCommand.FindOrAdd(Entry.CommandTag).Add(RuntimeEntry);
	}
}

void FActCommandRuntimeResolver::UnregisterAbilityChainWindow(const FName WindowId)
{
	if (WindowId.IsNone())
	{
		return;
	}

	TArray<FActAbilityChainEntry>* WindowEntries = ActiveChainWindows.Find(WindowId);
	if (!WindowEntries)
	{
		return;
	}

	for (const FActAbilityChainEntry& Entry : *WindowEntries)
	{
		TArray<FActAbilityChainRuntimeEntry>* RuntimeEntries = ActiveChainByCommand.Find(Entry.CommandTag);
		if (!RuntimeEntries)
		{
			continue;
		}

		RuntimeEntries->RemoveAll([&WindowId](const FActAbilityChainRuntimeEntry& RuntimeEntry)
		{
			return RuntimeEntry.WindowId == WindowId;
		});

		if (RuntimeEntries->IsEmpty())
		{
			ActiveChainByCommand.Remove(Entry.CommandTag);
		}
	}

	ActiveChainWindows.Remove(WindowId);
}

bool FActCommandRuntimeResolver::HasActiveAbilityChains() const
{
	return !ActiveChainByCommand.IsEmpty();
}

bool FActCommandRuntimeResolver::TryExecuteCommand(UActAbilitySystemComponent& ActASC, const FGameplayTag& CommandTag, const FGameplayTagContainer& OwnerTags)
{
	if (!CommandTag.IsValid())
	{
		return false;
	}
	
	const FGameplayAbilitySpec* CachedSpec = nullptr;
	if (CachedAbilityHandle.IsValid())
	{
		CachedSpec = ActASC.FindAbilitySpecFromHandle(CachedAbilityHandle);
		if (!CachedSpec)
		{
			ClearCachedAbility();
		}
	}

	if (CachedSpec && !CachedSpec->IsActive())
	{
		// Cached ability ended; clear and allow starter flow.
		ClearCachedAbility();
		CachedSpec = nullptr;
	}
	
	if (CachedSpec)
	{
		if (!HasActiveAbilityChains())
		{
			return false;
		}

		const TArray<FActAbilityChainRuntimeEntry>* RuntimeEntries = ActiveChainByCommand.Find(CommandTag);
		if (!RuntimeEntries || RuntimeEntries->IsEmpty())
		{
			return false;
		}

		const UActGameplayAbility* CachedAbilityCDO = Cast<UActGameplayAbility>(CachedSpec->Ability);
		const FName CachedAbilityId = CachedAbilityCDO ? CachedAbilityCDO->GetAbilityID() : NAME_None;

		auto IsEntrySameAsCached = [CachedAbilityId](const FActAbilityChainRuntimeEntry& RuntimeEntry) -> bool
		{
			return !CachedAbilityId.IsNone() && CachedAbilityId == RuntimeEntry.AbilityID;
		};

		for (int32 Index = RuntimeEntries->Num() - 1; Index >= 0; --Index)
		{
			const FActAbilityChainRuntimeEntry& RuntimeEntry = (*RuntimeEntries)[Index];
			if (IsEntrySameAsCached(RuntimeEntry))
			{
				continue;
			}

			FGameplayAbilitySpecHandle ActivatedSpecHandle;
			if (!ActASC.TryActivateAbilityByID(
				RuntimeEntry.AbilityID,
				/*bAllowRemoteActivation=*/true,
				/*bCancelIfAlreadyActive=*/false,
				&ActivatedSpecHandle))
			{
				continue;
			}

			CachedAbilityHandle = ActivatedSpecHandle;
			ClearAbilityChainWindows();
			return true;
		}

		// Only when the derived entry matches the cached ability itself do we cancel + replay.
		for (int32 Index = RuntimeEntries->Num() - 1; Index >= 0; --Index)
		{
			const FActAbilityChainRuntimeEntry& RuntimeEntry = (*RuntimeEntries)[Index];
			if (!IsEntrySameAsCached(RuntimeEntry))
			{
				continue;
			}

			if (CachedSpec->IsActive())
			{
				ActASC.CancelAbilityByHandle(CachedAbilityHandle);
			}

			FGameplayAbilitySpecHandle ActivatedSpecHandle;
			if (!ActASC.TryActivateAbilityByID(
				RuntimeEntry.AbilityID,
				/*bAllowRemoteActivation=*/true,
				/*bCancelIfAlreadyActive=*/false,
				&ActivatedSpecHandle))
			{
				continue;
			}

			CachedAbilityHandle = ActivatedSpecHandle;
			ClearAbilityChainWindows();
			return true;
		}

		return false;
	}

	FName StarterAbilityId;
	if (!ActASC.GetAbilityIdByInputTag(CommandTag, StarterAbilityId))
	{
		return false;
	}

	FGameplayAbilitySpecHandle ActivatedSpecHandle;
	if (ActASC.TryActivateAbilityByID(
		StarterAbilityId,
		/*bAllowRemoteActivation=*/true,
		/*bCancelIfAlreadyActive=*/false,
		&ActivatedSpecHandle))
	{
		CachedAbilityHandle = ActivatedSpecHandle;
		return true;
	}

	return false;
}
