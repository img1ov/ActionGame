#pragma once

// BulletSystem: BulletModel.h
// Runtime bullet state storage.

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Model/BulletInfo.h"
#include "BulletModel.generated.h"

class UBulletPool;

UCLASS()
class BULLETGAME_API UBulletModel : public UObject
{
    GENERATED_BODY()

public:
    // Runtime store for active bullets and their state.
    void Reserve(int32 Capacity);
    FBulletInfo* CreateBullet(UBulletPool* Pool, const FBulletInitParams& InitParams, const FBulletDataMain& Config);
    FBulletInfo* GetBullet(int32 BulletId);
    const TMap<int32, FBulletInfo>& GetBulletMap() const;
    TMap<int32, FBulletInfo>& GetMutableBulletMap();

    void MarkNeedDestroy(int32 BulletId);
    const TSet<int32>& GetNeedDestroyBullets() const;

    // Parent/child bullet relationship tracking.
    void GetChildBullets(int32 ParentBulletId, TArray<int32>& OutChildren) const;
    int32 GetParentBulletId(int32 ChildBulletId) const;

    void RemoveBullet(int32 BulletId);
    void Clear();

private:
    void RegisterChild(int32 ParentBulletId, int32 ChildBulletId);
    void UnregisterChild(int32 ChildBulletId);

    UPROPERTY()
    TMap<int32, FBulletInfo> BulletMap;

    TMap<int32, TSet<int32>> AttackerBullets;

    TMap<int32, TSet<int32>> ParentToChildren;
    TMap<int32, int32> ChildToParent;

    UPROPERTY()
    TSet<int32> NeedDestroyBullets;

    int32 NextBulletId = 1;
};

