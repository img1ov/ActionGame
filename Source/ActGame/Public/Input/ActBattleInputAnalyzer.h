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
	
	void AddInputTagPressed(const FGameplayTag& InputTag, double InputTimeSeconds, EInputDirection InputDirection, const FGameplayTagContainer& StateTags);
	void AddDirectionalInput(EInputDirection InputDirection, double InputTimeSeconds, const FGameplayTagContainer& StateTags);

	void ResetInputHistory();
	void ResetMatchedCommands();
	void Tick(double CurrentTimeSeconds);

	void SetCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions);
	void SetOnCommandMatchedDelegate(FActOnInputCommandMatched InOnCommandMatched);

	void GetInputHistory(TArray<FInputHistoryEntry>& OutEntries, bool bFromOldest = true) const;
	bool PeekMatchedCommand(double CurrentTimeSeconds, FGameplayTag& OutCommandTag);
	bool ConsumeMatchedCommand(double CurrentTimeSeconds, FGameplayTag& OutCommandTag);
	void BuildDebugLines(TArray<FString>& OutLines) const;

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
	void TryMatchCommands(const FInputHistoryEntry& LatestEntry);
	bool DoesStepMatchEntry(const FInputCommandStep& Step, const FInputHistoryEntry& Entry, double LastMatchedTimeSeconds) const;
	bool IsEntryEventTypeMatch(ECommandEventMatchType ExpectedEventType, EAnalyzerEventType ActualEventType) const;

private:
	
	static constexpr double MatchedCommandIdleTimeoutSeconds = 0.25;
	static constexpr double InputHistoryMaxAgeSeconds = 1.0;

	TArray<FInputCommandDefinition> CommandDefinitions;
	TFixedRingBuffer<FInputHistoryEntry, InputHistoryCapacity> InputHistory;
	TFixedRingBuffer<FMatchedCommandEntry, MatchedCommandCapacity> MatchedCommands;
	EInputDirection LastDirectionalInput = EInputDirection::Neutral;
	double LastInputTimeSeconds = -1.0;
	FGameplayTag LastEmittedCommandTag;
	double LastEmittedCommandTimeSeconds = -1.0;
	FActOnInputCommandMatched OnCommandMatched;
};
