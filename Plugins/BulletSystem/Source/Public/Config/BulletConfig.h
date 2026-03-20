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
    // Data tables that contribute rows of FBulletDataMain. Later entries override earlier ones by BulletID.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<TObjectPtr<UDataTable>> BulletDataTables;

    // Inline rows stored directly on the asset (useful for small test configs or quick overrides).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<FBulletDataMain> InlineBulletData;

    // Lookup a bullet row by id. RebuildRuntimeTable() will be called lazily if needed.
    bool GetBulletData(FName BulletID, FBulletDataMain& OutData) const;
    // Rebuild the runtime lookup table from BulletDataTables + InlineBulletData.
    void RebuildRuntimeTable() const;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    // Runtime lookup table for fast access (not UPROPERTY on purpose).
    mutable TMap<FName, FBulletDataMain> RuntimeTable;
    mutable bool bRuntimeTableDirty = true;
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

    // Assign the active config asset used by this game instance.
    void SetConfigAsset(UBulletConfig* InConfig);
    UBulletConfig* GetConfigAsset() const;

    // Resolve bullet data using:
    // - a common cache (shared rows),
    // - an owner cache (per owning actor id),
    // - and a class cache (per owner class) as a fallback.
    bool GetBulletData(FName BulletID, FBulletDataMain& OutData, const UObject* ContextOwner = nullptr);

    // Clears all resolved-row caches and (optionally) rebuilds the config asset runtime table.
    // Useful for PIE re-entry and editor iteration where DataTables are modified without restarting the game instance.
    void ClearCaches(bool bRebuildRuntimeTable = true);

    // Async-ish preloading loop (runs on game thread) to warm up soft references in queued bullet rows.
    void TickPreload(float DeltaSeconds);
    // Queue a bullet row for preload (idempotent per BulletID).
    void RequestPreload(const FBulletDataMain& Data);

private:
    UPROPERTY()
    TObjectPtr<UBulletConfig> ConfigAsset;

    // Cache for rows resolved without an owner context.
    UPROPERTY()
    TMap<FName, FBulletDataMain> CommonCache;

    // Cache partitioned by owner unique id (useful when per-owner overrides exist).
    UPROPERTY()
    TMap<int32, FBulletOwnerCache> OwnerCache;

    // Runtime cache; UObject key types are not supported by UPROPERTY.
    TMap<TObjectKey<UClass>, FBulletOwnerCache> ClassCache;

    // FIFO preload queue. Rows are de-duplicated using PreloadPendingRows.
    UPROPERTY()
    TArray<FBulletDataMain> PreloadQueue;

    UPROPERTY()
    TSet<FName> PreloadPendingRows;
};
