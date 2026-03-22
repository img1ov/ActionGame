// Copyright

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "BulletSystemTypes.h"

#include "ActBulletAnimNotifyPayload.generated.h"

/**
 * Lightweight payload object carried via GameplayEventData.OptionalObject.
 * This allows AnimNotifies to pass BulletID / SlotIndex / NotifyId to GA without custom net-serialization.
 *
 * Notes:
 * - Created locally on the sender (authority or autonomous proxy).
 * - Simulated proxies do not run GA, so they will not use this path.
 */
UCLASS(BlueprintType)
class ACTGAME_API UActBulletAnimNotifyPayload : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName BulletID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	FName NotifyId = NAME_None;

	// Optional extra parameters (used by ProcessManualHits / Destroy events).
	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bResetHitActorsBefore = true;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bApplyCollisionResponse = true;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	EBulletDestroyReason DestroyReason = EBulletDestroyReason::External;

	UPROPERTY(BlueprintReadOnly, Category = "Bullet")
	bool bSpawnChildren = true;
};
