// Copyright

#include "Input/ActBattleInputAnalyzer.h"

void FActBattleInputAnalyzer::AddInputTagPressed(const FGameplayTag& InputTag, const double InputTimeSeconds, const EInputDirection InputDirection, const FGameplayTagContainer& StateTags)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	FInputHistoryEntry Entry;
	Entry.InputTag = InputTag;
	Entry.InputTimeSeconds = InputTimeSeconds;
	Entry.EventType = EAnalyzerEventType::Pressed;
	Entry.Direction = InputDirection;
	Entry.StateTags = StateTags;

	AddInputEntry(Entry);
}

void FActBattleInputAnalyzer::AddDirectionalInput(const EInputDirection InputDirection, const double InputTimeSeconds, const FGameplayTagContainer& StateTags)
{
	if (InputDirection == LastDirectionalInput)
	{
		return;
	}

	LastDirectionalInput = InputDirection;

	FInputHistoryEntry Entry;
	Entry.InputTag = FGameplayTag();
	Entry.InputTimeSeconds = InputTimeSeconds;
	Entry.EventType = EAnalyzerEventType::Direction;
	Entry.Direction = InputDirection;
	Entry.StateTags = StateTags;

	AddInputEntry(Entry);
}

void FActBattleInputAnalyzer::ResetInputHistory()
{
	InputHistory.Reset();
	LastDirectionalInput = EInputDirection::Neutral;
	LastInputTimeSeconds = -1.0;
	LastEmittedCommandTag = FGameplayTag();
	LastEmittedCommandTimeSeconds = -1.0;
}

void FActBattleInputAnalyzer::ResetMatchedCommands()
{
	MatchedCommands.Reset();
}

void FActBattleInputAnalyzer::Tick(const double CurrentTimeSeconds)
{
	if (LastInputTimeSeconds < 0.0 || CurrentTimeSeconds < LastInputTimeSeconds)
	{
		return;
	}

	if ((CurrentTimeSeconds - LastInputTimeSeconds) > MatchedCommandIdleTimeoutSeconds)
	{
		ResetMatchedCommands();
	}
}

void FActBattleInputAnalyzer::SetCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions)
{
	CommandDefinitions = InCommandDefinitions;
}

void FActBattleInputAnalyzer::SetOnCommandMatchedDelegate(FActOnInputCommandMatched InOnCommandMatched)
{
	OnCommandMatched = InOnCommandMatched;
}

void FActBattleInputAnalyzer::GetInputHistory(TArray<FInputHistoryEntry>& OutEntries, const bool bFromOldest) const
{
	InputHistory.ToArray(OutEntries, bFromOldest);
}

bool FActBattleInputAnalyzer::ConsumeMatchedCommand(const double CurrentTimeSeconds, FGameplayTag& OutCommandTag)
{
	FMatchedCommandEntry Entry;
	while (MatchedCommands.TryPopOldest(Entry))
	{
		if (Entry.ExpireTimeSeconds < CurrentTimeSeconds)
		{
			continue;
		}

		OutCommandTag = Entry.CommandTag;
		return OutCommandTag.IsValid();
	}

	return false;
}

bool FActBattleInputAnalyzer::PeekMatchedCommand(const double CurrentTimeSeconds, FGameplayTag& OutCommandTag)
{
	while (!MatchedCommands.IsEmpty())
	{
		FMatchedCommandEntry Entry = MatchedCommands.GetOldest(0);
		if (Entry.ExpireTimeSeconds < CurrentTimeSeconds)
		{
			MatchedCommands.TryPopOldest(Entry);
			continue;
		}

		OutCommandTag = Entry.CommandTag;
		return OutCommandTag.IsValid();
	}

	return false;
}

void FActBattleInputAnalyzer::BuildDebugLines(TArray<FString>& OutLines) const
{
	OutLines.Reset();

	TArray<FInputHistoryEntry> Entries;
	GetInputHistory(Entries, true);

	OutLines.Reserve(Entries.Num() + 1);
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FInputHistoryEntry& Entry = Entries[Index];
		OutLines.Add(FString::Printf(
			TEXT("[%02d] E=%d Tag=%s Dir=%d Time=%.3f"),
			Index,
			static_cast<int32>(Entry.EventType),
			*Entry.InputTag.ToString(),
			static_cast<int32>(Entry.Direction),
			Entry.InputTimeSeconds));
	}

	OutLines.Add(FString::Printf(TEXT("MatchedCommands=%d"), MatchedCommands.Num()));
}

void FActBattleInputAnalyzer::AddInputEntry(const FInputHistoryEntry& InputEntry)
{
	Tick(InputEntry.InputTimeSeconds);
	LastInputTimeSeconds = InputEntry.InputTimeSeconds;
	InputHistory.Add(InputEntry);
	PruneExpiredInputHistory(InputEntry.InputTimeSeconds);
	TryMatchCommands(InputEntry);
}

void FActBattleInputAnalyzer::PruneExpiredInputHistory(const double CurrentTimeSeconds)
{
	TArray<FInputHistoryEntry> Entries;
	InputHistory.ToArray(Entries, true);
	if (Entries.IsEmpty())
	{
		return;
	}

	InputHistory.Reset();
	for (const FInputHistoryEntry& Entry : Entries)
	{
		// Keep recent samples only. This avoids stale inputs affecting a later command.
		if ((CurrentTimeSeconds - Entry.InputTimeSeconds) <= InputHistoryMaxAgeSeconds)
		{
			InputHistory.Add(Entry);
		}
	}
}

void FActBattleInputAnalyzer::TryMatchCommands(const FInputHistoryEntry& LatestEntry)
{
	if (InputHistory.IsEmpty())
	{
		return;
	}

	struct FMatchCandidate
	{
		FGameplayTag CommandTag;
		int32 Priority = TNumericLimits<int32>::Lowest();
		int32 StepCount = 0;
		double BufferLifetimeSeconds = 0.0;
	};

	FMatchCandidate BestMatch;

	for (const FInputCommandDefinition& CommandDefinition : CommandDefinitions)
	{
		if (!CommandDefinition.OutputCommandTag.IsValid() || CommandDefinition.Steps.IsEmpty())
		{
			continue;
		}

		if (!LatestEntry.StateTags.HasAll(CommandDefinition.RequiredStateTags))
		{
			continue;
		}

		// Edge-triggered matching: a command can only emit when the newest sample
		// satisfies the final step. This prevents re-emits from unrelated later input.
		const FInputCommandStep& FinalStep = CommandDefinition.Steps.Last();
		if (!DoesStepMatchEntry(FinalStep, LatestEntry, -1.0))
		{
			continue;
		}

		int32 StepIndex = CommandDefinition.Steps.Num() - 1;
		double LastMatchedTimeSeconds = -1.0;

		for (int32 OffsetFromLatest = 0; OffsetFromLatest < InputHistory.Num() && StepIndex >= 0; ++OffsetFromLatest)
		{
			const FInputHistoryEntry& Entry = InputHistory.GetLatest(OffsetFromLatest);
			const FInputCommandStep& Step = CommandDefinition.Steps[StepIndex];

			// Loose matching: unrelated events are skipped rather than treated as hard breaks.
			// This is intentional for action-game style input tolerance.
			if (!DoesStepMatchEntry(Step, Entry, LastMatchedTimeSeconds))
			{
				continue;
			}

			LastMatchedTimeSeconds = Entry.InputTimeSeconds;
			--StepIndex;
		}

		if (StepIndex >= 0)
		{
			continue;
		}

		if (CommandDefinition.Priority > BestMatch.Priority ||
			(CommandDefinition.Priority == BestMatch.Priority && CommandDefinition.Steps.Num() > BestMatch.StepCount))
		{
			BestMatch.CommandTag = CommandDefinition.OutputCommandTag;
			BestMatch.Priority = CommandDefinition.Priority;
			BestMatch.StepCount = CommandDefinition.Steps.Num();
			BestMatch.BufferLifetimeSeconds = FMath::Max(0.0, CommandDefinition.BufferLifetimeSeconds);
		}
	}

	if (BestMatch.CommandTag.IsValid())
	{
		if (BestMatch.CommandTag == LastEmittedCommandTag &&
			FMath::IsNearlyEqual(LastInputTimeSeconds, LastEmittedCommandTimeSeconds))
		{
			return;
		}

		FMatchedCommandEntry Entry;
		Entry.CommandTag = BestMatch.CommandTag;
		Entry.Priority = BestMatch.Priority;
		Entry.CreatedTimeSeconds = LastInputTimeSeconds;
		Entry.ExpireTimeSeconds = LastInputTimeSeconds + BestMatch.BufferLifetimeSeconds;
		MatchedCommands.Add(Entry);

		LastEmittedCommandTag = BestMatch.CommandTag;
		LastEmittedCommandTimeSeconds = LastInputTimeSeconds;

		OnCommandMatched.ExecuteIfBound(BestMatch.CommandTag);
	}
}

bool FActBattleInputAnalyzer::DoesStepMatchEntry(const FInputCommandStep& Step, const FInputHistoryEntry& Entry, const double LastMatchedTimeSeconds) const
{
	if (!IsEntryEventTypeMatch(Step.EventType, Entry.EventType))
	{
		return false;
	}

	if (Step.InputTag.IsValid() && Entry.InputTag != Step.InputTag)
	{
		return false;
	}

	if (Step.RequiredDirection != EInputDirection::Any && Entry.Direction != Step.RequiredDirection)
	{
		return false;
	}

	if (!Entry.StateTags.HasAll(Step.RequiredStateTags))
	{
		return false;
	}

	if (LastMatchedTimeSeconds >= 0.0)
	{
		const double DeltaTime = LastMatchedTimeSeconds - Entry.InputTimeSeconds;
		if (DeltaTime > Step.AllowedTimeGap)
		{
			return false;
		}
	}

	return true;
}

bool FActBattleInputAnalyzer::IsEntryEventTypeMatch(const ECommandEventMatchType ExpectedEventType, const EAnalyzerEventType ActualEventType) const
{
	switch (ExpectedEventType)
	{
		case ECommandEventMatchType::Any:
			return true;
		
		case ECommandEventMatchType::Pressed:
			return ActualEventType == EAnalyzerEventType::Pressed;
		
		case ECommandEventMatchType::Direction:
			return ActualEventType == EAnalyzerEventType::Direction;
		
		default:
			return false;
	}
}
