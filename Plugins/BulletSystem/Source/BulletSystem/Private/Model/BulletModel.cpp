#include "Model/BulletModel.h"
#include "Pool/BulletPools.h"
#include "Entity/BulletEntity.h"

FBulletInfo* UBulletModel::CreateBullet(UBulletPool* Pool, const FBulletInitParams& InitParams, const FBulletDataMain& Config)
{
    const int32 BulletId = NextBulletId++;

    FBulletInfo& Info = BulletMap.Add(BulletId);
    Info.BulletId = BulletId;
    Info.InitParams = InitParams;
    Info.Config = Config;
    Info.Tags = Config.Base.Tags;
    Info.LiveTime = 0.0f;
    Info.PendingDestroyDelay = 0.0f;
    Info.TimeScale = 1.0f;
    Info.bIsInit = false;
    Info.bNeedDestroy = false;
    Info.Size = (InitParams.SizeOverride.IsNearlyZero()) ? FVector::OneVector : InitParams.SizeOverride;

    if (Pool)
    {
        Info.Entity = Pool->AcquireEntity(this);
        if (Info.Entity)
        {
            Info.Entity->Initialize(BulletId);
        }
    }

    if (InitParams.Owner)
    {
        const int32 OwnerId = InitParams.Owner->GetUniqueID();
        AttackerBullets.FindOrAdd(OwnerId).Add(BulletId);
    }

    return &Info;
}

FBulletInfo* UBulletModel::GetBullet(int32 BulletId)
{
    return BulletMap.Find(BulletId);
}

const TMap<int32, FBulletInfo>& UBulletModel::GetBulletMap() const
{
    return BulletMap;
}

TMap<int32, FBulletInfo>& UBulletModel::GetMutableBulletMap()
{
    return BulletMap;
}

void UBulletModel::MarkNeedDestroy(int32 BulletId)
{
    NeedDestroyBullets.Add(BulletId);
}

const TSet<int32>& UBulletModel::GetNeedDestroyBullets() const
{
    return NeedDestroyBullets;
}

void UBulletModel::RemoveBullet(int32 BulletId)
{
    if (FBulletInfo* Info = BulletMap.Find(BulletId))
    {
        if (Info->InitParams.Owner)
        {
            const int32 OwnerId = Info->InitParams.Owner->GetUniqueID();
            if (TSet<int32>* Bullets = AttackerBullets.Find(OwnerId))
            {
                Bullets->Remove(BulletId);
                if (Bullets->Num() == 0)
                {
                    AttackerBullets.Remove(OwnerId);
                }
            }
        }
    }

    BulletMap.Remove(BulletId);
    NeedDestroyBullets.Remove(BulletId);
}

void UBulletModel::Clear()
{
    BulletMap.Empty();
    AttackerBullets.Empty();
    NeedDestroyBullets.Empty();
    NextBulletId = 1;
}
