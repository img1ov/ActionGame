#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletEntity.generated.h"

class UBulletActionLogicComponent;
class ABulletActor;

UCLASS()
class BULLETSYSTEM_API UBulletEntity : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(int32 InBulletId);
    void Reset();

    int32 GetBulletId() const;

    UBulletActionLogicComponent* GetLogicComponent() const;
    void SetActor(ABulletActor* InActor);
    ABulletActor* GetActor() const;

private:
    UPROPERTY()
    int32 BulletId = INDEX_NONE;

    UPROPERTY()
    TObjectPtr<UBulletActionLogicComponent> LogicComponent;

    UPROPERTY()
    TObjectPtr<ABulletActor> Actor;
};
