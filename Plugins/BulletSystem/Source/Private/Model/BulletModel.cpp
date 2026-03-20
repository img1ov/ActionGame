#include "Model/BulletModel.h"
#include "Pool/BulletPools.h"
#include "Entity/BulletEntity.h"

FBulletInfo* UBulletModel::SpawnBullet(UBulletPool* Pool, const FBulletInitParams& InitParams, const FBulletDataMain& Config)
{
    const int32 InstanceId = NextInstanceId++;

    FBulletInfo& Info = BulletMap.Add(InstanceId);
    Info.InstanceId = InstanceId;
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
    Info.ParentInstanceId = InitParams.ParentInstanceId;
    Info.DestroyWorldTime = -1.0f;
    Info.Size = (InitParams.SizeOverride.IsNearlyZero()) ? FVector::OneVector : InitParams.SizeOverride;

    if (Pool)
    {
        Info.Entity = Pool->AcquireEntity(this);
        if (Info.Entity)
        {
            Info.Entity->Initialize(InstanceId);
        }
    }

    if (InitParams.ParentInstanceId != INDEX_NONE)
    {
        RegisterChild(InitParams.ParentInstanceId, InstanceId);
    }

    if (InitParams.Owner)
    {
        const int32 OwnerId = InitParams.Owner->GetUniqueID();
        AttackerBullets.FindOrAdd(OwnerId).Add(InstanceId);
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

FBulletInfo* UBulletModel::GetBullet(int32 InstanceId)
{
    return BulletMap.Find(InstanceId);
}

const TMap<int32, FBulletInfo>& UBulletModel::GetBulletMap() const
{
    return BulletMap;
}

TMap<int32, FBulletInfo>& UBulletModel::GetMutableBulletMap()
{
    return BulletMap;
}

void UBulletModel::MarkNeedDestroy(int32 InstanceId)
{
    NeedDestroyBullets.Add(InstanceId);
}

const TSet<int32>& UBulletModel::GetNeedDestroyBullets() const
{
    return NeedDestroyBullets;
}

void UBulletModel::GetChildBullets(int32 ParentInstanceId, TArray<int32>& OutChildren) const
{
    OutChildren.Reset();
    if (const TSet<int32>* Children = ParentToChildren.Find(ParentInstanceId))
    {
        OutChildren.Reserve(Children->Num());
        for (int32 ChildInstanceId : *Children)
        {
            OutChildren.Add(ChildInstanceId);
        }
    }
}

int32 UBulletModel::GetParentInstanceId(int32 ChildInstanceId) const
{
    if (const int32* ParentId = ChildToParent.Find(ChildInstanceId))
    {
        return *ParentId;
    }
    return INDEX_NONE;
}

void UBulletModel::RemoveBullet(int32 InstanceId)
{
    if (FBulletInfo* Info = BulletMap.Find(InstanceId))
    {
        if (Info->InitParams.Owner)
        {
            const int32 OwnerId = Info->InitParams.Owner->GetUniqueID();
            if (TSet<int32>* Bullets = AttackerBullets.Find(OwnerId))
            {
                Bullets->Remove(InstanceId);
                if (Bullets->Num() == 0)
                {
                    AttackerBullets.Remove(OwnerId);
                }
            }
        }
    }

    UnregisterChild(InstanceId);
    ParentToChildren.Remove(InstanceId);

    BulletMap.Remove(InstanceId);
    NeedDestroyBullets.Remove(InstanceId);
}

void UBulletModel::Clear()
{
    BulletMap.Empty();
    AttackerBullets.Empty();
    NeedDestroyBullets.Empty();
    ParentToChildren.Empty();
    ChildToParent.Empty();
    NextInstanceId = 1;
}

void UBulletModel::RegisterChild(int32 ParentInstanceId, int32 ChildInstanceId)
{
    ParentToChildren.FindOrAdd(ParentInstanceId).Add(ChildInstanceId);
    ChildToParent.Add(ChildInstanceId, ParentInstanceId);
}

void UBulletModel::UnregisterChild(int32 ChildInstanceId)
{
    if (int32* ParentId = ChildToParent.Find(ChildInstanceId))
    {
        if (TSet<int32>* Children = ParentToChildren.Find(*ParentId))
        {
            Children->Remove(ChildInstanceId);
            if (Children->Num() == 0)
            {
                ParentToChildren.Remove(*ParentId);
            }
        }
        ChildToParent.Remove(ChildInstanceId);
    }
}
