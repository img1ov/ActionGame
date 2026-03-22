// BulletSystem: BulletConfig.cpp
// Config assets and settings.
#include "Config/BulletConfig.h"
#include "BulletLogChannels.h"
#if WITH_EDITOR
#include "UObject/UnrealType.h"
#endif

bool UBulletConfig::GetBulletData(FName BulletId, FBulletDataMain& OutData) const
{
    if (BulletId.IsNone())
    {
        return false;
    }

    if (bRuntimeTableDirty)
    {
        RebuildRuntimeTable();
    }

    if (const FBulletDataMain* Found = RuntimeTable.Find(BulletId))
    {
        OutData = *Found;
        OutData.BulletId = BulletId;
        return true;
    }

    return false;
}

void UBulletConfig::RebuildRuntimeTable() const
{
    RuntimeTable.Reset();
    bRuntimeTableDirty = false;

    int32 AddedRowCount = 0;
    for (UDataTable* Table : BulletDataTables)
    {
        if (!Table)
        {
            continue;
        }

        if (Table->GetRowStruct() != FBulletDataMain::StaticStruct())
        {
            UE_LOG(LogBullet, Warning, TEXT("BulletConfig: DataTable '%s' row struct mismatch. Expected=%s Actual=%s. Skipped."),
                *Table->GetName(),
                *GetNameSafe(FBulletDataMain::StaticStruct()),
                *GetNameSafe(Table->GetRowStruct()));
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

            FName RowBulletId = Row->BulletId;
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
                Added->BulletId = RowBulletId;
            }
            ++AddedRowCount;
        }
    }

    for (const FBulletDataMain& Item : InlineBulletData)
    {
        if (Item.BulletId.IsNone())
        {
            continue;
        }
        RuntimeTable.Add(Item.BulletId, Item);
        if (FBulletDataMain* Added = RuntimeTable.Find(Item.BulletId))
        {
            Added->BulletId = Item.BulletId;
        }
        ++AddedRowCount;
    }

    UE_LOG(LogBullet, VeryVerbose, TEXT("BulletConfig: RebuildRuntimeTable done. Rows=%d Tables=%d Inline=%d Asset=%s"),
        AddedRowCount, BulletDataTables.Num(), InlineBulletData.Num(), *GetNameSafe(this));
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
