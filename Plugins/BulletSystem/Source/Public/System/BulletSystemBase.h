#pragma once

// BulletSystem: BulletSystemBase.h
// High-frequency simulation systems (move/collision).

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletSystemBase.generated.h"

class UBulletController;

UCLASS(Abstract)
class BULLETSYSTEM_API UBulletSystemBase : public UObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(UBulletController* InController)
    {
        Controller = InController;
    }

    virtual void OnTick(float DeltaSeconds)
    {
        (void)DeltaSeconds;
    }

    virtual void OnAfterTick(float DeltaSeconds)
    {
        (void)DeltaSeconds;
    }

protected:
    UPROPERTY()
    TObjectPtr<UBulletController> Controller;
};

