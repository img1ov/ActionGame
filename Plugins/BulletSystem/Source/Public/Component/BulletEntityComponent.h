#pragma once

// BulletSystem: BulletEntityComponent.h
// Components used by bullet entities/actors.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletEntityComponent.generated.h"

class UBulletEntity;

// Base class for bullet entity components to align with the entity-style framework.
UCLASS(Abstract)
class BULLETSYSTEM_API UBulletEntityComponent : public UObject
{
    GENERATED_BODY()

public:
    virtual void InitializeComponent(UBulletEntity* InEntity);
    virtual void Reset();

    UBulletEntity* GetEntity() const;

protected:
    UPROPERTY()
    TObjectPtr<UBulletEntity> Entity;
};

