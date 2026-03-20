#pragma once

// BulletSystem: BulletEntity.h
// Lightweight runtime entity wrapper.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BulletEntity.generated.h"

class UBulletActionLogicComponent;
class UBulletActorComponent;
class UBulletBudgetComponent;
class UBulletEntityComponent;
class ABulletActor;

UCLASS()
class BULLETGAME_API UBulletEntity : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(int32 InInstanceId);
    void Reset();

    int32 GetInstanceId() const;

    UBulletActionLogicComponent* GetLogicComponent() const;
    UBulletActorComponent* GetActorComponent() const;
    UBulletBudgetComponent* GetBudgetComponent() const;
    void SetActor(ABulletActor* InActor) const;
    ABulletActor* GetActor() const;

    // Extension hook: allow adding custom components (advanced trajectory/collision features).
    UBulletEntityComponent* AddComponent(TSubclassOf<UBulletEntityComponent> ComponentClass);
    UBulletEntityComponent* FindComponentByClass(TSubclassOf<UBulletEntityComponent> ComponentClass) const;

private:
    void RegisterComponent(UBulletEntityComponent* Component);

    UPROPERTY()
    int32 InstanceId = INDEX_NONE;

    UPROPERTY()
    TObjectPtr<UBulletActionLogicComponent> LogicComponent;

    UPROPERTY()
    TObjectPtr<UBulletActorComponent> ActorComponent;

    UPROPERTY()
    TObjectPtr<UBulletBudgetComponent> BudgetComponent;

    UPROPERTY()
    TArray<TObjectPtr<UBulletEntityComponent>> Components;
};

