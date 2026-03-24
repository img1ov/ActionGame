#pragma once

#include "CoreMinimal.h"
#include "BulletSystemTypes.h"

#include "HitReactOptional.generated.h"

/**
 * Payload carried via GameplayEventData.OptionalObject for hit-react.
 *
 * This is intentionally Blueprint-friendly: GA can cast OptionalObject to UHitReactOptional
 * and read the per-hit impulse parameters directly.
 */
UCLASS(BlueprintType)
class BULLETSYSTEM_API UHitReactOptional : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "HitReact")
	FHitReactImpulse HitReactImpulse;
};

