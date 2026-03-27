// Copyright

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ActBattleInputAnalyzer.generated.h"

UENUM(BlueprintType)
enum class EAnalyzerEventType : uint8
{
	Pressed,
	Direction
};

UENUM(BlueprintType)
enum class ECommandEventMatchType : uint8
{
	Any = 0,
	Pressed = 1,
	Direction = 3
};

UENUM(BlueprintType)
enum class EInputDirection : uint8
{
	Any,
	Neutral,
	Forward,
	Backward,
	Left,
	Right,
	ForwardLeft,
	ForwardRight,
	BackwardLeft,
	BackwardRight
};

/**
 * Single input sample saved into analyzer history.
 * This is a context-aware event sample with direction and state snapshot.
 */
USTRUCT(BlueprintType)
struct FInputHistoryEntry
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag = FGameplayTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	double InputTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EAnalyzerEventType EventType = EAnalyzerEventType::Pressed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EInputDirection Direction = EInputDirection::Neutral;

	/** Ability/character state snapshot when this input happened. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FGameplayTagContainer StateTags;

};

/**
 * One step in a command definition.
 * Example: 236P can be represented as [Down, DownForward, Forward, Punch].
 */
USTRUCT(BlueprintType)
struct FInputCommandStep
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag = FGameplayTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	ECommandEventMatchType EventType = ECommandEventMatchType::Any;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	EInputDirection RequiredDirection = EInputDirection::Any;

	/** Max delta time to previous matched step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.0"))
	double AllowedTimeGap = 0.25;

	/** Extra state requirements for this step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FGameplayTagContainer RequiredStateTags;

};

/**
 * Command definition and output command tag.
 * If all steps match in order, OutputCommandTag will be emitted.
 */
USTRUCT(BlueprintType)
struct FInputCommandDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputCommand"))
	FGameplayTag OutputCommandTag = FGameplayTag();

	/** Higher value wins when multiple commands match on the same input event. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;

	/** How long this command can stay in buffer before it expires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0.0"))
	double BufferLifetimeSeconds = 0.25;

	/** Command-level required state tags. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FGameplayTagContainer RequiredStateTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TArray<FInputCommandStep> Steps;
};

template<typename T, int32 Capacity>
struct TFixedRingBuffer
{
	T Data[Capacity];
	int32 Head = 0;
	int32 NumItems = 0;

	FORCEINLINE void Add(const T& Item)
	{
		Data[Head] = Item;
		Head = (Head + 1) % Capacity;
		NumItems = FMath::Min(NumItems + 1, Capacity);
	}

	FORCEINLINE void Reset()
	{
		Head = 0;
		NumItems = 0;
	}

	FORCEINLINE int32 Num() const { return NumItems; }
	FORCEINLINE bool IsEmpty() const { return NumItems == 0; }

	FORCEINLINE int32 GetRealIndexFromOldest(const int32 LogicalIndex) const
	{
		check(LogicalIndex >= 0 && LogicalIndex < NumItems);
		return (Head - NumItems + LogicalIndex + Capacity) % Capacity;
	}

	FORCEINLINE int32 GetRealIndexFromLatest(const int32 OffsetFromLatest) const
	{
		check(OffsetFromLatest >= 0 && OffsetFromLatest < NumItems);
		return (Head - 1 - OffsetFromLatest + Capacity) % Capacity;
	}

	FORCEINLINE const T& GetOldest(const int32 LogicalIndex = 0) const
	{
		return Data[GetRealIndexFromOldest(LogicalIndex)];
	}

	FORCEINLINE const T& GetLatest(const int32 Offset = 0) const
	{
		return Data[GetRealIndexFromLatest(Offset)];
	}

	FORCEINLINE bool TryPopOldest(T& OutItem)
	{
		if (NumItems <= 0)
		{
			return false;
		}

		const int32 OldestIndex = (Head - NumItems + Capacity) % Capacity;
		OutItem = Data[OldestIndex];
		--NumItems;
		return true;
	}

	FORCEINLINE void ToArray(TArray<T>& OutArray, const bool bFromOldest = true) const
	{
		OutArray.Reset(NumItems);
		OutArray.Reserve(NumItems);

		if (bFromOldest)
		{
			for (int32 i = 0; i < NumItems; ++i)
			{
				OutArray.Add(GetOldest(i));
			}
			return;
		}

		for (int32 i = 0; i < NumItems; ++i)
		{
			OutArray.Add(GetLatest(i));
		}
	}
};

DECLARE_DELEGATE_OneParam(FActOnInputCommandMatched, const FGameplayTag&);

/**
 * Runtime input analyzer.
 * Designed to live in PlayerController and evaluate command sequences from input history.
 */
class ACTGAME_API FActBattleInputAnalyzer
{
public:
	
	static constexpr int32 InputHistoryCapacity = 48;
	static constexpr int32 MatchedCommandCapacity = 8;

public:
	
	/**
	 * Records a "pressed" style input event into history and attempts to match commands.
	 *
	 * @param InputTag Ability input tag that was pressed (e.g. InputTag.Attack.Light).
	 * @param InputTimeSeconds World time when event occurred.
	 * @param InputDirection Quantized character-relative direction at that time.
	 * @param StateTags Snapshot of gameplay state tags at that time (used for conditional commands).
	 */
	void AddInputTagPressed(const FGameplayTag& InputTag, double InputTimeSeconds, EInputDirection InputDirection, const FGameplayTagContainer& StateTags);

	/**
	 * Records a directional input change (WASD / stick direction).
	 * This can be used for quarter-circles or directional preconditions.
	 */
	void AddDirectionalInput(EInputDirection InputDirection, double InputTimeSeconds, const FGameplayTagContainer& StateTags);

	/** Clears the input history ring buffer. */
	void ResetInputHistory();
	/** Clears the matched command ring buffer (already emitted commands). */
	void ResetMatchedCommands();
	/** Clears both history and matched commands. */
	void ResetCommandBuffer();
	/** Per-frame housekeeping: prunes expired history and command buffers. */
	void Tick(double CurrentTimeSeconds);

	/** Updates the list of command definitions the analyzer will attempt to match. */
	void SetCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions);
	/** Callback invoked when a command is matched and added into the command buffer. */
	void SetOnCommandMatchedDelegate(FActOnInputCommandMatched InOnCommandMatched);

	/** Returns the input history for debugging/UI. */
	void GetInputHistory(TArray<FInputHistoryEntry>& OutEntries, bool bFromOldest = true) const;
	/** Peeks the oldest buffered matched command if it is still alive. */
	bool PeekMatchedCommand(double CurrentTimeSeconds, FGameplayTag& OutCommandTag);
	/** Pops the oldest buffered matched command if it is still alive. */
	bool ConsumeMatchedCommand(double CurrentTimeSeconds, FGameplayTag& OutCommandTag);
	/** Builds debug lines to help visualize what matched/why. */
	void BuildDebugLines(TArray<FString>& OutLines, const FGameplayTag& CommandTag = FGameplayTag()) const;

private:
	
	struct FMatchedCommandEntry
	{
		FGameplayTag CommandTag;
		int32 Priority = 0;
		double CreatedTimeSeconds = 0.0;
		double ExpireTimeSeconds = 0.0;
	};

	void AddInputEntry(const FInputHistoryEntry& InputEntry);
	void PruneExpiredInputHistory(double CurrentTimeSeconds);
	void PruneExpiredMatchedCommands(double CurrentTimeSeconds);
	void TryMatchCommands(const FInputHistoryEntry& LatestEntry);
	bool DoesStepMatchEntry(const FInputCommandStep& Step, const FInputHistoryEntry& Entry, double LastMatchedTimeSeconds) const;
	bool IsEntryEventTypeMatch(ECommandEventMatchType ExpectedEventType, EAnalyzerEventType ActualEventType) const;

private:
	
	static constexpr double InputHistoryMaxAgeSeconds = 1.0;

	/** Authoritative command definitions (usually configured from PawnData). */
	TArray<FInputCommandDefinition> CommandDefinitions;
	/** Ring buffer of recent input samples. */
	TFixedRingBuffer<FInputHistoryEntry, InputHistoryCapacity> InputHistory;
	/** Ring buffer of matched command outputs (acts as a command buffer). */
	TFixedRingBuffer<FMatchedCommandEntry, MatchedCommandCapacity> MatchedCommands;
	/** Last known directional input (used to coalesce/avoid duplicate events). */
	EInputDirection LastDirectionalInput = EInputDirection::Neutral;
	/** Last input time; used for pruning and debug. */
	double LastInputTimeSeconds = -1.0;
	/** De-dup guard for emitting the same command repeatedly in the same frame window. */
	FGameplayTag LastEmittedCommandTag;
	/** Time when LastEmittedCommandTag was pushed. */
	double LastEmittedCommandTimeSeconds = -1.0;
	/** External listener (usually PlayerController) notified on match. */
	FActOnInputCommandMatched OnCommandMatched;
};
