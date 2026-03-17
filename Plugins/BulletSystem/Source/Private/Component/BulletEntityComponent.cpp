// BulletSystem: BulletEntityComponent.cpp
// Components used by bullet entities/actors.
#include "Component/BulletEntityComponent.h"
#include "Entity/BulletEntity.h"

void UBulletEntityComponent::InitializeComponent(UBulletEntity* InEntity)
{
    Entity = InEntity;
}

void UBulletEntityComponent::Reset()
{
    // Base reset keeps entity reference; subclasses clear their own state.
}

UBulletEntity* UBulletEntityComponent::GetEntity() const
{
    return Entity;
}

