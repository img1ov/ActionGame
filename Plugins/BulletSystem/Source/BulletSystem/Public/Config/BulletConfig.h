#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BulletSystemTypes.h"
#include "BulletConfig.generated.h"

UCLASS(BlueprintType)
class BULLETSYSTEM_API UBulletConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TObjectPtr<UDataTable> BulletDataTable;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<FBulletDataMain> InlineBulletData;

    bool GetBulletData(FName RowName, FBulletDataMain& OutData) const;
};

USTRUCT()
struct FBulletOwnerCache
{
    GENERATED_BODY()

    UPROPERTY()
    TMap<FName, FBulletDataMain> Rows;
};

UCLASS()
class BULLETSYSTEM_API UBulletConfigSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void SetConfigAsset(UBulletConfig* InConfig);
    UBulletConfig* GetConfigAsset() const;

    bool GetBulletData(FName RowName, FBulletDataMain& OutData, const UObject* ContextOwner = nullptr);

    void TickPreload(float DeltaSeconds);

private:
    UPROPERTY()
    TObjectPtr<UBulletConfig> ConfigAsset;

    UPROPERTY()
    TMap<FName, FBulletDataMain> CommonCache;

    UPROPERTY()
    TMap<int32, FBulletOwnerCache> OwnerCache;
};
