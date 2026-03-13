#pragma once

#include "CoreMinimal.h"
#include "System/BulletSystemBase.h"
#include "BulletCollisionSystem.generated.h"

UCLASS()
class BULLETSYSTEM_API UBulletCollisionSystem : public UBulletSystemBase
{
    GENERATED_BODY()

public:
    // High-frequency collision and hit processing system.
    virtual void OnTick(float DeltaSeconds) override;
};
