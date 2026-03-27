#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"

#include "ActAbilityChainNotifies.generated.h"

UCLASS(meta = (DisplayName = "Act AbilityChain Node"))
class ACTGAME_API UAN_AbilityChainNode : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain")
	FName NodeId;
};

UCLASS(meta = (DisplayName = "Act AbilityChain Window"))
class ACTGAME_API UANS_AbilityChainWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AbilityChain", meta = (Categories = "AbilityChain.Window"))
	FGameplayTag WindowTag;
};
