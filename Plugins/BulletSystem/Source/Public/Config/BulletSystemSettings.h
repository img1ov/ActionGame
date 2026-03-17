#pragma once

// BulletSystem: BulletSystemSettings.h
// Config assets and settings.

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BulletSystemSettings.generated.h"

class UBulletConfig;
class ABulletActor;

UCLASS(Config = Game, DefaultConfig)
class BULLETGAME_API UBulletSystemSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    TSoftObjectPtr<UBulletConfig> DefaultConfigAsset;

    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    TSoftClassPtr<ABulletActor> DefaultBulletActorClass;

    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    bool bEnableDebugDraw = false;

    // Pre-allocate bullet storage to reduce TMap rehash during burst spawns.
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem", meta = (ClampMin = "0"))
    int32 InitialBulletCapacity = 512;
};

