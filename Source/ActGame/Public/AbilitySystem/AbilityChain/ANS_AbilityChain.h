// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AbilitySystem/AbilityChain/ActAbilityChainTypes.h"

#include "ANS_AbilityChain.generated.h"

UCLASS(meta = (DisplayName = "Ability Chain Window"))
class ACTGAME_API UANS_AbilityChain : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (TitleProperty = "CommandTag"))
	TArray<FActAbilityChainEntry> ChainEntries;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

private:
	// Unique ID per notify instance for runtime window tracking.
	UPROPERTY(VisibleAnywhere, Category = "AbilityChain")
	FName WindowId;
};
