#pragma once

// BulletSystem: BulletInteractInterface.h
// Interaction interface for scene hooks.

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Engine/EngineTypes.h"
#include "BulletInteractInterface.generated.h"

UINTERFACE(BlueprintType)
class BULLETSYSTEM_API UBulletInteractInterface : public UInterface
{
    GENERATED_BODY()
};

class BULLETSYSTEM_API IBulletInteractInterface
{
    GENERATED_BODY()

public:
    // Called when a bullet with Interact enabled hits this actor.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "BulletSystem")
    void OnBulletInteract(const FBulletInfo& BulletInfo, const FHitResult& Hit);
};

