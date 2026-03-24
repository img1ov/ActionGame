// Copyright

#include "Input/ActCommandRuntimeResolver.h"

#include "ActLogChannels.h"
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
	int32 SkippedEntries = 0;

	for (const FActAbilityChainEntry& Entry : ChainEntries)
	{
		if (!Entry.CommandTag.IsValid() || Entry.AbilityID.IsNone())
		{
			++SkippedEntries;
			continue;
		}

		ValidEntries.Add(Entry);
	}

	if (ValidEntries.IsEmpty())
	{
		if (SkippedEntries > 0)
		{
			UE_LOG(LogActAbilitySystem, Verbose, TEXT("AbilityChain window [%s] skipped %d invalid entries; no valid entries remain."),
				*WindowId.ToString(), SkippedEntries);
		}
		return;
	}
	if (SkippedEntries > 0)
	{
		UE_LOG(LogActAbilitySystem, Verbose, TEXT("AbilityChain window [%s] skipped %d invalid entries."),
			*WindowId.ToString(), SkippedEntries);
	}

	ActiveChainWindows.Add(WindowId, ValidEntries);

	for (const FActAbilityChainEntry& Entry : ValidEntries)
	{
		FActAbilityChainRuntimeEntry RuntimeEntry;
		RuntimeEntry.CommandTag = Entry.CommandTag;
		RuntimeEntry.AbilityID = Entry.AbilityID;
		RuntimeEntry.WindowId = WindowId;
		RuntimeEntry.Priority = Entry.Priority;
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

		// Prefer higher priority when multiple entries match the same command.
		// Tie-breaker keeps the old behavior: later (more recently registered) entries win.
		struct FCandidate
		{
			int32 Index = INDEX_NONE;
			int32 Priority = 0;
		};

		TArray<FCandidate> Candidates;
		Candidates.Reserve(RuntimeEntries->Num());
		for (int32 Index = 0; Index < RuntimeEntries->Num(); ++Index)
		{
			FCandidate Candidate;
			Candidate.Index = Index;
			Candidate.Priority = (*RuntimeEntries)[Index].Priority;
			Candidates.Add(Candidate);
		}

		Candidates.Sort([](const FCandidate& A, const FCandidate& B)
		{
			if (A.Priority != B.Priority)
			{
				return A.Priority > B.Priority;
			}
			return A.Index > B.Index;
		});

		for (const FCandidate& Candidate : Candidates)
		{
			const FActAbilityChainRuntimeEntry& RuntimeEntry = (*RuntimeEntries)[Candidate.Index];

			// Note: We intentionally do not cancel the currently active cached ability here.
			// If the best match resolves to the same ability, retrigger behavior (if desired)
			// should be authored on the ability asset itself (e.g. bRetriggerInstancedAbility).
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
		UE_LOG(LogActAbilitySystem, VeryVerbose, TEXT("TryExecuteCommand: No ability mapped for command tag [%s]."),
			*CommandTag.ToString());
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
