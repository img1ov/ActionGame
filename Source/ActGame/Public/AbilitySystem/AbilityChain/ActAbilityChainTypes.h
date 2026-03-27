#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ActAbilityChainTypes.generated.h"

/**
 * One authored follow-up entry inside a combo window.
 *
 * This is the atomic authoring unit of the first-generation combo model:
 * a command tag maps directly to one follow-up ability Id.
 */
USTRUCT(BlueprintType)
struct FActAbilityChainEntry
{
	GENERATED_BODY()

public:
	/** Input command emitted by the battle input analyzer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "InputCommand"))
	FGameplayTag CommandTag;

	/** Stable authored Id of the follow-up ability that should activate on a match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName TargetAbilityId;

	/** Higher value wins when multiple entries inside the same window match the same command. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0"))
	int32 Priority = 0;

	/** True if this entry is fully authored and can participate in runtime resolution. */
	bool IsValid() const
	{
		return CommandTag.IsValid() && !TargetAbilityId.IsNone();
	}
};

/**
 * Full authored data carried by one AnimNotifyState combo window.
 *
 * Each notify window is self-contained:
 * - one window-level priority for overlap resolution
 * - one list of follow-up entries for command matching
 */
USTRUCT(BlueprintType)
struct FActAbilityChainWindowDefinition
{
	GENERATED_BODY()

public:
	/** Stable runtime identity of the authored window instance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName WindowId;

	/** Higher value wins when overlapping windows compete for the same command. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0"))
	int32 WindowPriority = 0;

	/** Candidate follow-up entries available while this window is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (TitleProperty = "TargetAbilityId"))
	TArray<FActAbilityChainEntry> Entries;

	/** True if the window can be registered into the runtime. */
	bool IsValid() const
	{
		return !WindowId.IsNone() && !Entries.IsEmpty();
	}
};

/**
 * Lightweight client-to-server prediction request for combo follow-up authorization.
 *
 * The client predicts which ability should chain next, and the server re-validates that
 * the current active windows still permit that command -> ability pair.
 */
USTRUCT(BlueprintType)
struct FActAbilityChainActivationRequest
{
	GENERATED_BODY()

public:
	/** Source ability currently owning the open combo windows. */
	UPROPERTY()
	FName SourceAbilityId;

	/** Input command that produced the predicted follow-up. */
	UPROPERTY()
	FGameplayTag CommandTag;

	/** Ability Id the client resolved locally and wants the server to authorize. */
	UPROPERTY()
	FName TargetAbilityId;

	/** True if the request contains enough information for server-side validation. */
	bool IsValid() const
	{
		return !SourceAbilityId.IsNone() && CommandTag.IsValid() && !TargetAbilityId.IsNone();
	}
};
