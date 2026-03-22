#pragma once

// BulletSystem: BulletConfig.h
// Config assets and settings.

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "BulletSystemTypes.h"
#include "BulletConfig.generated.h"

UCLASS(BlueprintType)
class BULLETGAME_API UBulletConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // Data tables that contribute rows of FBulletDataMain. Later entries override earlier ones by BulletId.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<TObjectPtr<UDataTable>> BulletDataTables;

    // Inline rows stored directly on the asset (useful for small test configs or quick overrides).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config")
    TArray<FBulletDataMain> InlineBulletData;

    // Lookup a bullet row by id. RebuildRuntimeTable() will be called lazily if needed.
    bool GetBulletData(FName BulletId, FBulletDataMain& OutData) const;
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
