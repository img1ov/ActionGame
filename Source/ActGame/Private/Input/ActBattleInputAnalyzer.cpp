// Copyright

#include "Input/ActBattleInputAnalyzer.h"

namespace
{
const TCHAR* ToEventTypeString(const ECommandEventMatchType Type)
{
	switch (Type)
	{
		case ECommandEventMatchType::Any: return TEXT("Any");
		case ECommandEventMatchType::Pressed: return TEXT("Pressed");
		case ECommandEventMatchType::Direction: return TEXT("Direction");
		default: return TEXT("Unknown");
	}
}

const TCHAR* ToDirectionString(const EInputDirection Dir)
{
	switch (Dir)
	{
		case EInputDirection::Any: return TEXT("Any");
		case EInputDirection::Neutral: return TEXT("Neutral");
		case EInputDirection::Forward: return TEXT("Forward");
		case EInputDirection::Backward: return TEXT("Backward");
		case EInputDirection::Left: return TEXT("Left");
		case EInputDirection::Right: return TEXT("Right");
		case EInputDirection::ForwardLeft: return TEXT("ForwardLeft");
		case EInputDirection::ForwardRight: return TEXT("ForwardRight");
		case EInputDirection::BackwardLeft: return TEXT("BackwardLeft");
		case EInputDirection::BackwardRight: return TEXT("BackwardRight");
		default: return TEXT("Unknown");
	}
}

FString ToTagsString(const FGameplayTagContainer& Tags)
{
	if (Tags.IsEmpty())
	{
		return TEXT("None");
	}

	return Tags.ToString();
}
}

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

void FActBattleInputAnalyzer::ResetCommandBuffer()
{
	ResetMatchedCommands();
	ResetInputHistory();
}

void FActBattleInputAnalyzer::Tick(const double CurrentTimeSeconds)
{
	if (LastInputTimeSeconds < 0.0 || CurrentTimeSeconds < LastInputTimeSeconds)
	{
		return;
	}

	PruneExpiredMatchedCommands(CurrentTimeSeconds);
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

void FActBattleInputAnalyzer::BuildDebugLines(TArray<FString>& OutLines, const FGameplayTag& CommandTag) const
{
	OutLines.Reset();

	if (CommandTag.IsValid())
	{
		const FInputCommandDefinition* Definition = nullptr;
		for (const FInputCommandDefinition& CommandDefinition : CommandDefinitions)
		{
			if (CommandDefinition.OutputCommandTag == CommandTag)
			{
				Definition = &CommandDefinition;
				break;
			}
		}

		if (!Definition)
		{
			OutLines.Add(FString::Printf(
				TEXT("[BattleCommandResolver] Command=%s (definition not found)"),
				*CommandTag.ToString()));
			return;
		}

		OutLines.Reserve(Definition->Steps.Num() + 1);
		OutLines.Add(FString::Printf(
			TEXT("[BattleCommandResolver] Command=%s Priority=%d Steps=%d Buffer=%.3fs RequiredTags=%s"),
			*Definition->OutputCommandTag.ToString(),
			Definition->Priority,
			Definition->Steps.Num(),
			Definition->BufferLifetimeSeconds,
			*ToTagsString(Definition->RequiredStateTags)));

		for (int32 StepIndex = 0; StepIndex < Definition->Steps.Num(); ++StepIndex)
		{
			const FInputCommandStep& Step = Definition->Steps[StepIndex];
			OutLines.Add(FString::Printf(
				TEXT("  Step[%d] Tag=%s Event=%s Dir=%s Gap<=%.3f RequiredTags=%s"),
				StepIndex,
				*Step.InputTag.ToString(),
				ToEventTypeString(Step.EventType),
				ToDirectionString(Step.RequiredDirection),
				Step.AllowedTimeGap,
				*ToTagsString(Step.RequiredStateTags)));
		}

		return;
	}

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
	if (InputHistory.IsEmpty())
	{
		return;
	}

	const int32 NumItems = InputHistory.Num();
	FInputHistoryEntry KeptEntries[InputHistoryCapacity];
	int32 KeptCount = 0;

	for (int32 Index = 0; Index < NumItems; ++Index)
	{
		const FInputHistoryEntry& Entry = InputHistory.GetOldest(Index);
		// Keep recent samples only. This avoids stale inputs affecting a later command.
		if ((CurrentTimeSeconds - Entry.InputTimeSeconds) <= InputHistoryMaxAgeSeconds)
		{
			KeptEntries[KeptCount++] = Entry;
		}
	}

	if (KeptCount == NumItems)
	{
		return;
	}

	InputHistory.Reset();
	for (int32 Index = 0; Index < KeptCount; ++Index)
	{
		InputHistory.Add(KeptEntries[Index]);
	}
}

void FActBattleInputAnalyzer::PruneExpiredMatchedCommands(const double CurrentTimeSeconds)
{
	if (MatchedCommands.IsEmpty())
	{
		return;
	}

	while (!MatchedCommands.IsEmpty())
	{
		const FMatchedCommandEntry Entry = MatchedCommands.GetOldest(0);
		if (Entry.ExpireTimeSeconds >= CurrentTimeSeconds)
		{
			break;
		}

		FMatchedCommandEntry Discarded;
		MatchedCommands.TryPopOldest(Discarded);
	}
}

void FActBattleInputAnalyzer::TryMatchCommands(const FInputHistoryEntry& LatestEntry)
{
	if (InputHistory.IsEmpty())
	{
		return;
	}
	if (CommandDefinitions.IsEmpty())
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

	const int32 HistoryNum = InputHistory.Num();
	for (const FInputCommandDefinition& CommandDefinition : CommandDefinitions)
	{
		if (!CommandDefinition.OutputCommandTag.IsValid())
		{
			continue;
		}

		const int32 StepCount = CommandDefinition.Steps.Num();
		if (StepCount == 0)
		{
			continue;
		}

		if (!LatestEntry.StateTags.HasAll(CommandDefinition.RequiredStateTags))
		{
			continue;
		}

		// Edge-triggered matching: a command can only emit when the newest sample
		// satisfies the final step. This prevents re-emits from unrelated later input.
		//
		// Example:
		// - "LightAttack" might be a single pressed step.
		// - Without edge-trigger, any later directional event could repeatedly re-match it.
		const FInputCommandStep& FinalStep = CommandDefinition.Steps.Last();
		if (!DoesStepMatchEntry(FinalStep, LatestEntry, -1.0))
		{
			continue;
		}

		int32 StepIndex = StepCount - 1;
		double LastMatchedTimeSeconds = -1.0;

		for (int32 OffsetFromLatest = 0; OffsetFromLatest < HistoryNum && StepIndex >= 0; ++OffsetFromLatest)
		{
			const FInputHistoryEntry& Entry = InputHistory.GetLatest(OffsetFromLatest);
			const FInputCommandStep& Step = CommandDefinition.Steps[StepIndex];

			// Loose matching: unrelated events are skipped rather than treated as hard breaks.
			// This is intentional for action-game style input tolerance (you can wiggle stick without
			// destroying a partially-entered command).
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
			(CommandDefinition.Priority == BestMatch.Priority && StepCount > BestMatch.StepCount))
		{
			BestMatch.CommandTag = CommandDefinition.OutputCommandTag;
			BestMatch.Priority = CommandDefinition.Priority;
			BestMatch.StepCount = StepCount;
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

		// Command buffering policy:
		// we keep only the newest accepted command (single-slot buffer).
		// This avoids ambiguous multi-command queues and matches common action-game feel.
		MatchedCommands.Reset();
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
