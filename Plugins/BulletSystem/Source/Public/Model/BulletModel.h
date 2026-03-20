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
    FBulletInfo* SpawnBullet(UBulletPool* Pool, const FBulletInitParams& InitParams, const FBulletDataMain& Config);
    FBulletInfo* GetBullet(int32 InstanceId);
    const TMap<int32, FBulletInfo>& GetBulletMap() const;
    TMap<int32, FBulletInfo>& GetMutableBulletMap();

    // Mark a bullet for destruction at the end of the frame (controller will flush these).
    void MarkNeedDestroy(int32 InstanceId);
    const TSet<int32>& GetNeedDestroyBullets() const;

    // Parent/child bullet relationship tracking.
    void GetChildBullets(int32 ParentInstanceId, TArray<int32>& OutChildren) const;
    int32 GetParentInstanceId(int32 ChildInstanceId) const;

    void RemoveBullet(int32 InstanceId);
    void Clear();

private:
    void RegisterChild(int32 ParentInstanceId, int32 ChildInstanceId);
    void UnregisterChild(int32 ChildInstanceId);

    // Primary runtime storage. Key is the runtime-generated bullet id.
    UPROPERTY()
    TMap<int32, FBulletInfo> BulletMap;

    // Map from owner unique id to bullets spawned/owned by that actor (used for queries and cleanup).
    TMap<int32, TSet<int32>> AttackerBullets;

    // Parent->children relationship for hierarchical bullets (child bullets inherit/track parent lifecycle).
    TMap<int32, TSet<int32>> ParentToChildren;
    TMap<int32, int32> ChildToParent;

    // Bullets pending destruction at end-of-tick. Stored separately to avoid mutating BulletMap while iterating.
    UPROPERTY()
    TSet<int32> NeedDestroyBullets;

    // Monotonic id generator. Starts at 1 (0/INDEX_NONE reserved).
    int32 NextInstanceId = 1;
};

