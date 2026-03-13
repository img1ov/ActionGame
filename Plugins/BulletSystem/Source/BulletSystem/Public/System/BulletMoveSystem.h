#pragma once

#include "CoreMinimal.h"
#include "System/BulletSystemBase.h"
#include "BulletMoveSystem.generated.h"

UCLASS()
class BULLETSYSTEM_API UBulletMoveSystem : public UBulletSystemBase
{
    GENERATED_BODY()

public:
    // High-frequency movement update system.
    virtual void OnTick(float DeltaSeconds) override;
};
