// BulletSystem: BulletConfig.cpp
// Config assets and settings.
#include "Config/BulletConfig.h"
#include "Config/BulletSystemSettings.h"
#include "BulletLogChannel.h"
#include "Logic/BulletLogicData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

bool UBulletConfig::GetBulletData(FName BulletID, FBulletDataMain& OutData) const
{
    if (BulletID.IsNone())
    {
        return false;
    }

    if (bRuntimeTableDirty)
    {
        RebuildRuntimeTable();
    }

    if (const FBulletDataMain* Found = RuntimeTable.Find(BulletID))
    {
        OutData = *Found;
        OutData.BulletID = BulletID;
        return true;
    }

    return false;
}

void UBulletConfig::RebuildRuntimeTable() const
{
    RuntimeTable.Reset();
    bRuntimeTableDirty = false;

    for (UDataTable* Table : BulletDataTables)
    {
        if (!Table)
        {
            continue;
        }

        if (Table->GetRowStruct() != FBulletDataMain::StaticStruct())
        {
            UE_LOG(LogBullet, Warning, TEXT("BulletConfig: DataTable '%s' row struct mismatch (expected FBulletDataMain). Skipped."), *Table->GetName());
            continue;
        }

        const TMap<FName, uint8*>& RowMap = Table->GetRowMap();
        for (const TPair<FName, uint8*>& Pair : RowMap)
        {
            const FName RowName = Pair.Key;
            const FBulletDataMain* Row = reinterpret_cast<const FBulletDataMain*>(Pair.Value);
            if (!Row)
            {
                continue;
            }

            FName RowBulletId = Row->BulletID;
            if (RowBulletId.IsNone())
            {
                RowBulletId = RowName;
            }

            if (RowBulletId.IsNone())
            {
                continue;
            }

            RuntimeTable.Add(RowBulletId, *Row);
            if (FBulletDataMain* Added = RuntimeTable.Find(RowBulletId))
            {
                Added->BulletID = RowBulletId;
            }
        }
    }

    for (const FBulletDataMain& Item : InlineBulletData)
    {
        if (Item.BulletID.IsNone())
        {
            continue;
        }
        RuntimeTable.Add(Item.BulletID, Item);
        if (FBulletDataMain* Added = RuntimeTable.Find(Item.BulletID))
        {
            Added->BulletID = Item.BulletID;
        }
    }
}

#if WITH_EDITOR
void UBulletConfig::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UBulletConfig, BulletDataTables) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(UBulletConfig, InlineBulletData) ||
        PropertyName == GET_MEMBER_NAME_CHECKED(FBulletDataMain, ConfigProfile))
    {
        for (FBulletDataMain& Item : InlineBulletData)
        {
            Item.ApplyConfigProfileDefaults();
        }
        bRuntimeTableDirty = true;
    }
}
#endif

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
        UE_LOG(LogBullet, Warning, TEXT("BulletConfigSubsystem: No default config asset set."));
    }
    else
    {
        ConfigAsset->RebuildRuntimeTable();
    }
}

void UBulletConfigSubsystem::Deinitialize()
{
    CommonCache.Empty();
    OwnerCache.Empty();
    ClassCache.Empty();
    PreloadQueue.Empty();
    PreloadPendingRows.Empty();
    ConfigAsset = nullptr;

    Super::Deinitialize();
}

void UBulletConfigSubsystem::SetConfigAsset(UBulletConfig* InConfig)
{
    ConfigAsset = InConfig;
    if (ConfigAsset)
    {
        ConfigAsset->RebuildRuntimeTable();
    }
    CommonCache.Empty();
    OwnerCache.Empty();
    ClassCache.Empty();
    PreloadQueue.Empty();
    PreloadPendingRows.Empty();
}

UBulletConfig* UBulletConfigSubsystem::GetConfigAsset() const
{
    return ConfigAsset;
}

bool UBulletConfigSubsystem::GetBulletData(FName BulletID, FBulletDataMain& OutData, const UObject* ContextOwner)
{
    if (BulletID.IsNone())
    {
        return false;
    }

    // Cache precedence (highest wins):
    // 1) Per-owner (unique id) cache
    // 2) Per-owner-class cache
    // 3) Common cache (no owner context)
    const int32 OwnerKey = ContextOwner ? ContextOwner->GetUniqueID() : 0;
    if (OwnerKey != 0)
    {
        if (FBulletOwnerCache* OwnerMap = OwnerCache.Find(OwnerKey))
        {
            if (FBulletDataMain* Cached = OwnerMap->Rows.Find(BulletID))
            {
                OutData = *Cached;
                return true;
            }
        }
    }

    if (ContextOwner)
    {
        // Per-class cache is useful when different pawn/weapon classes map bullet ids to different rows.
        TObjectKey<UClass> OwnerClassKey(ContextOwner->GetClass());
        if (FBulletOwnerCache* ClassMap = ClassCache.Find(OwnerClassKey))
        {
            if (FBulletDataMain* Cached = ClassMap->Rows.Find(BulletID))
            {
                OutData = *Cached;
                return true;
            }
        }
    }

    if (FBulletDataMain* Cached = CommonCache.Find(BulletID))
    {
        OutData = *Cached;
        return true;
    }

    if (!ConfigAsset)
    {
        UE_LOG(LogBullet, Warning, TEXT("BulletConfigSubsystem: Config asset missing when looking for bullet '%s'."), *BulletID.ToString());
        return false;
    }

    FBulletDataMain Loaded;
    if (!ConfigAsset->GetBulletData(BulletID, Loaded))
    {
        UE_LOG(LogBullet, Warning, TEXT("BulletConfigSubsystem: Bullet '%s' not found."), *BulletID.ToString());
        return false;
    }

    CommonCache.Add(BulletID, Loaded);
    if (OwnerKey != 0)
    {
        OwnerCache.FindOrAdd(OwnerKey).Rows.Add(BulletID, Loaded);
    }
    if (ContextOwner)
    {
        TObjectKey<UClass> OwnerClassKey(ContextOwner->GetClass());
        ClassCache.FindOrAdd(OwnerClassKey).Rows.Add(BulletID, Loaded);
    }
    OutData = Loaded;
    return true;
}

void UBulletConfigSubsystem::TickPreload(float DeltaSeconds)
{
    // Lightweight asset warmup to reduce first-use hitches.
    (void)DeltaSeconds;

    if (PreloadQueue.Num() == 0)
    {
        return;
    }

    FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
    constexpr int32 MaxPerTick = 2;
    int32 Processed = 0;

    while (PreloadQueue.Num() > 0 && Processed++ < MaxPerTick)
    {
        const FBulletDataMain Data = PreloadQueue[0];
        PreloadQueue.RemoveAt(0, 1, EAllowShrinking::No);
        PreloadPendingRows.Remove(Data.BulletID);

        for (const TSoftObjectPtr<UBulletLogicData>& LogicDataPtr : Data.Execution.LogicDataList)
        {
            const FSoftObjectPath Path = LogicDataPtr.ToSoftObjectPath();
            if (Path.IsValid())
            {
                Streamable.RequestAsyncLoad(Path);
            }
        }

        const FSoftObjectPath NiagaraPath = Data.Render.NiagaraSystem.ToSoftObjectPath();
        if (NiagaraPath.IsValid())
        {
            Streamable.RequestAsyncLoad(NiagaraPath);
        }

        const FSoftObjectPath MeshPath = Data.Render.StaticMesh.ToSoftObjectPath();
        if (MeshPath.IsValid())
        {
            Streamable.RequestAsyncLoad(MeshPath);
        }
    }
}

void UBulletConfigSubsystem::RequestPreload(const FBulletDataMain& Data)
{
    if (Data.BulletID.IsNone())
    {
        return;
    }

    if (PreloadPendingRows.Contains(Data.BulletID))
    {
        return;
    }

    PreloadPendingRows.Add(Data.BulletID);
    PreloadQueue.Add(Data);
}

