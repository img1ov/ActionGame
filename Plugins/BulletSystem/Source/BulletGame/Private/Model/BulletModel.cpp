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
    Info.bIsSimple = Config.CheckSimpleBullet();
    Info.SpawnWorldTime = 0.0f;
    Info.ParentBulletId = InitParams.ParentBulletId;
    Info.DestroyWorldTime = -1.0f;
    Info.Size = (InitParams.SizeOverride.IsNearlyZero()) ? FVector::OneVector : InitParams.SizeOverride;

    if (Pool)
    {
        Info.Entity = Pool->AcquireEntity(this);
        if (Info.Entity)
        {
            Info.Entity->Initialize(BulletId);
        }
    }

    if (InitParams.ParentBulletId != INDEX_NONE)
    {
        RegisterChild(InitParams.ParentBulletId, BulletId);
    }

    if (InitParams.Owner)
    {
        const int32 OwnerId = InitParams.Owner->GetUniqueID();
        AttackerBullets.FindOrAdd(OwnerId).Add(BulletId);
    }

    return &Info;
}

void UBulletModel::Reserve(int32 Capacity)
{
    if (Capacity > 0)
    {
        BulletMap.Reserve(Capacity);
    }
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

void UBulletModel::GetChildBullets(int32 ParentBulletId, TArray<int32>& OutChildren) const
{
    OutChildren.Reset();
    if (const TSet<int32>* Children = ParentToChildren.Find(ParentBulletId))
    {
        OutChildren.Reserve(Children->Num());
        for (int32 ChildId : *Children)
        {
            OutChildren.Add(ChildId);
        }
    }
}

int32 UBulletModel::GetParentBulletId(int32 ChildBulletId) const
{
    if (const int32* ParentId = ChildToParent.Find(ChildBulletId))
    {
        return *ParentId;
    }
    return INDEX_NONE;
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

    UnregisterChild(BulletId);
    ParentToChildren.Remove(BulletId);

    BulletMap.Remove(BulletId);
    NeedDestroyBullets.Remove(BulletId);
}

void UBulletModel::Clear()
{
    BulletMap.Empty();
    AttackerBullets.Empty();
    NeedDestroyBullets.Empty();
    ParentToChildren.Empty();
    ChildToParent.Empty();
    NextBulletId = 1;
}

void UBulletModel::RegisterChild(int32 ParentBulletId, int32 ChildBulletId)
{
    ParentToChildren.FindOrAdd(ParentBulletId).Add(ChildBulletId);
    ChildToParent.Add(ChildBulletId, ParentBulletId);
}

void UBulletModel::UnregisterChild(int32 ChildBulletId)
{
    if (int32* ParentId = ChildToParent.Find(ChildBulletId))
    {
        if (TSet<int32>* Children = ParentToChildren.Find(*ParentId))
        {
            Children->Remove(ChildBulletId);
            if (Children->Num() == 0)
            {
                ParentToChildren.Remove(*ParentId);
            }
        }
        ChildToParent.Remove(ChildBulletId);
    }
}
