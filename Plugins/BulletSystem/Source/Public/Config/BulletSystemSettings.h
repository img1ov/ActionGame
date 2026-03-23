#pragma once

// BulletSystem: BulletSystemSettings.h
// Config assets and settings.

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BulletSystemSettings.generated.h"

class ABulletActor;

UENUM()
enum class EBulletRuntimeResetPolicy : uint8
{
    None,
    BeginPlayOnly,
    StopPlayOnly,
    BeginPlayAndStopPlay,
};

UCLASS(Config = Game, DefaultConfig)
class BULLETSYSTEM_API UBulletSystemSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    TSoftClassPtr<ABulletActor> DefaultBulletActorClass;

    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    bool bEnableDebugDraw = false;

    // Pre-allocate bullet storage to reduce TMap rehash during burst spawns.
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem", meta = (ClampMin = "0"))
    int32 InitialBulletCapacity = 512;

    // When to reset bullet runtime state (pools + live bullets) for this world.
    // In PIE the editor can reuse world/subsystem instances between Play sessions, so enabling StopPlay reset avoids stale pools.
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem|Runtime")
    EBulletRuntimeResetPolicy RuntimeResetPolicy = EBulletRuntimeResetPolicy::BeginPlayAndStopPlay;
};
