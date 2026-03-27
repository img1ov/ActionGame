#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ActAbilityChainTypes.generated.h"

UENUM(BlueprintType)
enum class EActAbilityChainActivationMode : uint8
{
	Ignore,
	StarterOnly,
	ChainOnly,
	StarterOrChain
};

UENUM(BlueprintType)
enum class EActAbilityChainTransitionType : uint8
{
	Section,
	Ability
};

USTRUCT(BlueprintType)
struct FActAbilityChainTransition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "InputCommand"))
	FGameplayTag CommandTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "AbilityChain.Window"))
	FGameplayTagContainer RequiredWindowTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "AbilityChain.Window"))
	FGameplayTagContainer BlockedWindowTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FGameplayTagContainer RequiredOwnerTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FGameplayTagContainer BlockedOwnerTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (ClampMin = "0"))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	bool bConsumeInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	bool bCancelCurrentAbility = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	EActAbilityChainTransitionType TransitionType = EActAbilityChainTransitionType::Section;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (EditCondition = "TransitionType == EActAbilityChainTransitionType::Section"))
	FName TargetNodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (EditCondition = "TransitionType == EActAbilityChainTransitionType::Ability"))
	FName TargetAbilityID;
};

USTRUCT(BlueprintType)
struct FActAbilityChainNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName SectionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (TitleProperty = "CommandTag"))
	TArray<FActAbilityChainTransition> Transitions;
};

USTRUCT(BlueprintType)
struct FActAbilityChainReplicatedTransition
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName SourceAbilityID;

	UPROPERTY()
	FName SourceNodeId;

	UPROPERTY()
	FName TargetAbilityID;

	UPROPERTY()
	FName TargetNodeId;

	UPROPERTY()
	bool bCancelCurrentAbility = false;

	bool IsValid() const
	{
		return !SourceNodeId.IsNone() && (!TargetNodeId.IsNone() || !TargetAbilityID.IsNone());
	}

	bool IsSectionTransition() const
	{
		return !TargetNodeId.IsNone();
	}

	bool IsAbilityTransition() const
	{
		return !TargetAbilityID.IsNone();
	}
};
