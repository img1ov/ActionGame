// BulletSystem: BulletWorldSubsystem.cpp
// Runtime controller layer and orchestration.
#include "Controller/BulletWorldSubsystem.h"
#include "Controller/BulletController.h"
#include "Config/BulletSystemSettings.h"
#include "BulletLogChannels.h"
#include "Engine/World.h"
#include "GameDelegates.h"

void UBulletWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UBulletWorldSubsystem::HandleWorldCleanup);

    Controller = NewObject<UBulletController>(this);
    if (Controller)
    {
        Controller->Initialize(GetWorld());
    }

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    const EBulletRuntimeResetPolicy ResetPolicy = Settings ? Settings->RuntimeResetPolicy : EBulletRuntimeResetPolicy::BeginPlayAndStopPlay;
    if (ResetPolicy == EBulletRuntimeResetPolicy::StopPlayOnly || ResetPolicy == EBulletRuntimeResetPolicy::BeginPlayAndStopPlay)
    {
        if (!EndPlayMapHandle.IsValid())
        {
            EndPlayMapHandle = FGameDelegates::Get().GetEndPlayMapDelegate().AddUObject(this, &UBulletWorldSubsystem::HandleEndPlayMap);
        }
    }
}

void UBulletWorldSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    if (EndPlayMapHandle.IsValid())
    {
        FGameDelegates::Get().GetEndPlayMapDelegate().Remove(EndPlayMapHandle);
        EndPlayMapHandle.Reset();
    }

    if (Controller)
    {
        Controller->Shutdown();
        Controller = nullptr;
    }

    Super::Deinitialize();
}

void UBulletWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    if (&InWorld != GetWorld())
    {
        return;
    }

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    const EBulletRuntimeResetPolicy ResetPolicy = Settings ? Settings->RuntimeResetPolicy : EBulletRuntimeResetPolicy::BeginPlayAndStopPlay;

    // PIE sessions can reuse world/subsystem instances. Ensure pools are clean at BeginPlay if configured.
    if (ResetPolicy == EBulletRuntimeResetPolicy::BeginPlayOnly || ResetPolicy == EBulletRuntimeResetPolicy::BeginPlayAndStopPlay)
    {
        UE_LOG(LogBullet, Verbose, TEXT("BulletWorldSubsystem: BeginPlay reset. World=%s Policy=%d"), *InWorld.GetName(), static_cast<int32>(ResetPolicy));
        if (Controller)
        {
            Controller->Shutdown();
        }
    }
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

void UBulletWorldSubsystem::HandleWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
    (void)bSessionEnded;
    (void)bCleanupResources;

    if (!InWorld || InWorld != GetWorld())
    {
        return;
    }

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    const EBulletRuntimeResetPolicy ResetPolicy = Settings ? Settings->RuntimeResetPolicy : EBulletRuntimeResetPolicy::BeginPlayAndStopPlay;

    if (ResetPolicy == EBulletRuntimeResetPolicy::StopPlayOnly || ResetPolicy == EBulletRuntimeResetPolicy::BeginPlayAndStopPlay)
    {
        UE_LOG(LogBullet, Verbose, TEXT("BulletWorldSubsystem: WorldCleanup reset. World=%s Policy=%d SessionEnded=%s CleanupResources=%s"),
            *InWorld->GetName(),
            static_cast<int32>(ResetPolicy),
            bSessionEnded ? TEXT("true") : TEXT("false"),
            bCleanupResources ? TEXT("true") : TEXT("false"));
        if (Controller)
        {
            Controller->Shutdown();
        }
    }
}

void UBulletWorldSubsystem::HandleEndPlayMap()
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    const EBulletRuntimeResetPolicy ResetPolicy = Settings ? Settings->RuntimeResetPolicy : EBulletRuntimeResetPolicy::BeginPlayAndStopPlay;
    if (ResetPolicy != EBulletRuntimeResetPolicy::StopPlayOnly && ResetPolicy != EBulletRuntimeResetPolicy::BeginPlayAndStopPlay)
    {
        return;
    }

    UE_LOG(LogBullet, Verbose, TEXT("BulletWorldSubsystem: EndPlayMap reset. World=%s Policy=%d"), *World->GetName(), static_cast<int32>(ResetPolicy));
    if (Controller)
    {
        Controller->Shutdown();
    }
}
