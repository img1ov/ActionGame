// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "BulletSystemTypes.h"

#include "AN_DestroyBullet.generated.h"

/**
 * Destroy bullet AnimNotify.
 * Thin forwarder into UActBulletComponent.
 */
UCLASS(meta = (DisplayName = "Destroy Bullet"))
class ACTGAME_API UAN_DestroyBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet", meta = (ClampMin = "-1"))
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	EBulletDestroyReason Reason = EBulletDestroyReason::External;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	bool bSpawnChildren = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};

