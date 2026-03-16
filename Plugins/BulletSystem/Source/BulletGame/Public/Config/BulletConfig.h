#pragma once

// BulletSystem: BulletConfig.h
// Config assets and settings.

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectKey.h"
#include "BulletSystemTypes.h"
#include "BulletConfig.generated.h"

UCLASS(BlueprintType)
class BULLETGAME_API UBulletConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TObjectPtr<UDataTable> BulletDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<FBulletDataMain> InlineBulletData;

    bool GetBulletData(FName BulletID, FBulletDataMain& OutData) const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

USTRUCT()
struct FBulletOwnerCache
{
    GENERATED_BODY()

    UPROPERTY()
    TMap<FName, FBulletDataMain> Rows;
};

UCLASS()
class BULLETGAME_API UBulletConfigSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void SetConfigAsset(UBulletConfig* InConfig);
    UBulletConfig* GetConfigAsset() const;

    bool GetBulletData(FName BulletID, FBulletDataMain& OutData, const UObject* ContextOwner = nullptr);

    void TickPreload(float DeltaSeconds);
    void RequestPreload(const FBulletDataMain& Data);

private:
    UPROPERTY()
    TObjectPtr<UBulletConfig> ConfigAsset;

    UPROPERTY()
    TMap<FName, FBulletDataMain> CommonCache;

    UPROPERTY()
    TMap<int32, FBulletOwnerCache> OwnerCache;

    // Runtime cache; UObject key types are not supported by UPROPERTY.
    TMap<TObjectKey<UClass>, FBulletOwnerCache> ClassCache;

    UPROPERTY()
    TArray<FBulletDataMain> PreloadQueue;

    UPROPERTY()
    TSet<FName> PreloadPendingRows;
};

