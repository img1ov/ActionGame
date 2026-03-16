// BulletSystem: BulletWorldSubsystem.cpp
// Runtime controller layer and orchestration.
#include "Controller/BulletWorldSubsystem.h"
#include "Controller/BulletController.h"

void UBulletWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Controller = NewObject<UBulletController>(this);
    if (Controller)
    {
        Controller->Initialize(GetWorld());
    }
}

void UBulletWorldSubsystem::Deinitialize()
{
    if (Controller)
    {
        Controller->Shutdown();
        Controller = nullptr;
    }

    Super::Deinitialize();
}

void UBulletWorldSubsystem::Tick(float DeltaSeconds)
{
    if (Controller)
    {
        Controller->OnTick(DeltaSeconds);
        Controller->OnAfterTick(DeltaSeconds);
    }
}

TStatId UBulletWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletWorldSubsystem, STATGROUP_Tickables);
}

UBulletController* UBulletWorldSubsystem::GetController() const
{
    return Controller;
}

