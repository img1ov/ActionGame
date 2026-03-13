#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BulletSystemSettings.generated.h"

class UBulletConfig;
class ABulletActor;

UCLASS(Config = Game, DefaultConfig)
class BULLETSYSTEM_API UBulletSystemSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    TSoftObjectPtr<UBulletConfig> DefaultConfigAsset;

    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    TSoftClassPtr<ABulletActor> DefaultBulletActorClass;

    UPROPERTY(Config, EditAnywhere, Category = "BulletSystem")
    bool bEnableDebugDraw = false;
};
