#pragma once

// BulletSystem: BulletSystemInterface.h
// Minimal interface used by the Blueprint library to locate a UBulletSystemComponent from any actor.

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "BulletSystemInterface.generated.h"

class UBulletSystemComponent;

UINTERFACE(BlueprintType)
class BULLETGAME_API UBulletSystemInterface : public UInterface
{
	GENERATED_BODY()
};

class BULLETGAME_API IBulletSystemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "BulletSystem")
	UBulletSystemComponent* GetBulletSystemComponent() const;
};

