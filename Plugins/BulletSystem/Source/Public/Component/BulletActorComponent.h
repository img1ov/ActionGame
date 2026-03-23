#pragma once

// BulletSystem: BulletActorComponent.h
// Components used by bullet entities/actors.

#include "CoreMinimal.h"
#include "Component/BulletEntityComponent.h"
#include "BulletActorComponent.generated.h"

class ABulletActor;

UCLASS()
class BULLETSYSTEM_API UBulletActorComponent : public UBulletEntityComponent
{
    GENERATED_BODY()

public:
    virtual void Reset() override;

    void SetActor(ABulletActor* InActor);
    ABulletActor* GetActor() const;

private:
    UPROPERTY()
    TObjectPtr<ABulletActor> Actor;
};

