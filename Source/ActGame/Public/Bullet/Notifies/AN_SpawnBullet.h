// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"

#include "AN_SpawnBullet.generated.h"

/**
 * Spawn bullet AnimNotify.
 * This notify is intentionally thin: it forwards to UActBulletComponent.
 */
UCLASS(meta = (DisplayName = "Spawn Bullet"))
class ACTGAME_API UAN_SpawnBullet : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet", meta = (ClampMin = "-1"))
	int32 SlotIndex = 0;

	/** Optional socket to use as spawn transform basis. If None or missing, uses mesh component transform. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FName SpawnSocketName = NAME_None;

	/** Gameplay event tag to route to GA (authority/autonomous only). Simulated proxies spawn locally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|GA")
	FGameplayTag SpawnEventTag;

	/** Simulated proxy uses collision to drive local hit VFX. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|SimProxy")
	bool bForceCollisionOnSimProxy = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
