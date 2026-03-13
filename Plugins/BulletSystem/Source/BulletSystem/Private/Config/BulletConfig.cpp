#include "Config/BulletConfig.h"
#include "Config/BulletSystemSettings.h"
#include "BulletSystemLog.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

bool UBulletConfig::GetBulletData(FName RowName, FBulletDataMain& OutData) const
{
    if (!RowName.IsNone() && BulletDataTable)
    {
        const FBulletDataMain* Row = BulletDataTable->FindRow<FBulletDataMain>(RowName, TEXT("BulletConfig"));
        if (Row)
        {
            OutData = *Row;
            OutData.RowName = RowName;
            return true;
        }
    }

    for (const FBulletDataMain& Item : InlineBulletData)
    {
        if (Item.RowName == RowName)
        {
            OutData = Item;
            return true;
        }
    }

    return false;
}

void UBulletConfigSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UBulletSystemSettings* Settings = GetDefault<UBulletSystemSettings>();
    if (Settings && Settings->DefaultConfigAsset.IsValid())
    {
        ConfigAsset = Settings->DefaultConfigAsset.Get();
    }
    else if (Settings && Settings->DefaultConfigAsset.ToSoftObjectPath().IsValid())
    {
        FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
        ConfigAsset = Cast<UBulletConfig>(Streamable.LoadSynchronous(Settings->DefaultConfigAsset.ToSoftObjectPath()));
    }

    if (!ConfigAsset)
    {
        UE_LOG(LogBulletSystem, Warning, TEXT("BulletConfigSubsystem: No default config asset set."));
    }
}

void UBulletConfigSubsystem::Deinitialize()
{
    CommonCache.Empty();
    OwnerCache.Empty();
    ConfigAsset = nullptr;

    Super::Deinitialize();
}

void UBulletConfigSubsystem::SetConfigAsset(UBulletConfig* InConfig)
{
    ConfigAsset = InConfig;
    CommonCache.Empty();
    OwnerCache.Empty();
}

UBulletConfig* UBulletConfigSubsystem::GetConfigAsset() const
{
    return ConfigAsset;
}

bool UBulletConfigSubsystem::GetBulletData(FName RowName, FBulletDataMain& OutData, const UObject* ContextOwner)
{
    if (RowName.IsNone())
    {
        return false;
    }

    const int32 OwnerKey = ContextOwner ? ContextOwner->GetUniqueID() : 0;
    if (OwnerKey != 0)
    {
        if (FBulletOwnerCache* OwnerMap = OwnerCache.Find(OwnerKey))
        {
            if (FBulletDataMain* Cached = OwnerMap->Rows.Find(RowName))
            {
                OutData = *Cached;
                return true;
            }
        }
    }

    if (FBulletDataMain* Cached = CommonCache.Find(RowName))
    {
        OutData = *Cached;
        return true;
    }

    if (!ConfigAsset)
    {
        UE_LOG(LogBulletSystem, Warning, TEXT("BulletConfigSubsystem: Config asset missing when looking for row '%s'."), *RowName.ToString());
        return false;
    }

    FBulletDataMain Loaded;
    if (!ConfigAsset->GetBulletData(RowName, Loaded))
    {
        UE_LOG(LogBulletSystem, Warning, TEXT("BulletConfigSubsystem: Bullet row '%s' not found."), *RowName.ToString());
        return false;
    }

    CommonCache.Add(RowName, Loaded);
    if (OwnerKey != 0)
    {
        OwnerCache.FindOrAdd(OwnerKey).Rows.Add(RowName, Loaded);
    }
    OutData = Loaded;
    return true;
}

void UBulletConfigSubsystem::TickPreload(float DeltaSeconds)
{
    // Placeholder for async preload or cache warmup.
    (void)DeltaSeconds;
}
