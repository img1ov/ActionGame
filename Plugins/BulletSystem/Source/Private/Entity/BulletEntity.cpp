// BulletSystem: BulletEntity.cpp
// Lightweight runtime entity wrapper.
#include "Entity/BulletEntity.h"
#include "Component/BulletActionLogicComponent.h"
#include "Component/BulletActorComponent.h"
#include "Component/BulletBudgetComponent.h"
#include "Component/BulletEntityComponent.h"

void UBulletEntity::Initialize(int32 InInstanceId)
{
    InstanceId = InInstanceId;
    if (!LogicComponent)
    {
        LogicComponent = NewObject<UBulletActionLogicComponent>(this);
    }
    if (!ActorComponent)
    {
        ActorComponent = NewObject<UBulletActorComponent>(this);
    }
    if (!BudgetComponent)
    {
        BudgetComponent = NewObject<UBulletBudgetComponent>(this);
    }
    RegisterComponent(LogicComponent);
    RegisterComponent(ActorComponent);
    RegisterComponent(BudgetComponent);

    for (UBulletEntityComponent* Component : Components)
    {
        if (Component)
        {
            Component->Reset();
        }
    }
}

void UBulletEntity::Reset()
{
    InstanceId = INDEX_NONE;
    for (UBulletEntityComponent* Component : Components)
    {
        if (Component)
        {
            Component->Reset();
        }
    }
}

int32 UBulletEntity::GetInstanceId() const
{
    return InstanceId;
}

UBulletActionLogicComponent* UBulletEntity::GetLogicComponent() const
{
    return LogicComponent;
}

UBulletActorComponent* UBulletEntity::GetActorComponent() const
{
    return ActorComponent;
}

UBulletBudgetComponent* UBulletEntity::GetBudgetComponent() const
{
    return BudgetComponent;
}

void UBulletEntity::SetActor(ABulletActor* InActor) const
{
    if (ActorComponent)
    {
        ActorComponent->SetActor(InActor);
    }
}

ABulletActor* UBulletEntity::GetActor() const
{
    return ActorComponent ? ActorComponent->GetActor() : nullptr;
}

UBulletEntityComponent* UBulletEntity::AddComponent(TSubclassOf<UBulletEntityComponent> ComponentClass)
{
    if (!ComponentClass)
    {
        return nullptr;
    }

    UBulletEntityComponent* Component = NewObject<UBulletEntityComponent>(this, ComponentClass);
    RegisterComponent(Component);
    if (Component)
    {
        Component->Reset();
    }
    return Component;
}

UBulletEntityComponent* UBulletEntity::FindComponentByClass(TSubclassOf<UBulletEntityComponent> ComponentClass) const
{
    if (!ComponentClass)
    {
        return nullptr;
    }

    for (UBulletEntityComponent* Component : Components)
    {
        if (Component && Component->IsA(ComponentClass))
        {
            return Component;
        }
    }

    return nullptr;
}

void UBulletEntity::RegisterComponent(UBulletEntityComponent* Component)
{
    if (!Component)
    {
        return;
    }

    Component->InitializeComponent(this);

    if (!Components.Contains(Component))
    {
        Components.Add(Component);
    }
}

