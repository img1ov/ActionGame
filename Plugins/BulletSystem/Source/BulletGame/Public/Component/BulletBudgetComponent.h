#pragma once

// BulletSystem: BulletBudgetComponent.h
// Components used by bullet entities/actors.

#include "CoreMinimal.h"
#include "Component/BulletEntityComponent.h"
#include "BulletBudgetComponent.generated.h"

UCLASS()
class BULLETGAME_API UBulletBudgetComponent : public UBulletEntityComponent
{
    GENERATED_BODY()

public:
    void Initialize(float InInterval, float SpawnTime);
    virtual void Reset() override;

    bool ConsumeMoveTick(float WorldTime, float DefaultDelta, float& OutDelta);
    bool ConsumeCollisionTick(float WorldTime, float DefaultDelta, float& OutDelta);

private:
    float BudgetInterval = 0.0f;
    float LastMoveTickTime = -BIG_NUMBER;
    float LastCollisionTickTime = -BIG_NUMBER;
};

