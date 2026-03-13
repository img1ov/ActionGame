#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Model/BulletInfo.h"
#include "BulletModel.generated.h"

class UBulletPool;

UCLASS()
class BULLETSYSTEM_API UBulletModel : public UObject
{
    GENERATED_BODY()

public:
    // Runtime store for active bullets and their state.
    FBulletInfo* CreateBullet(UBulletPool* Pool, const FBulletInitParams& InitParams, const FBulletDataMain& Config);
    FBulletInfo* GetBullet(int32 BulletId);
    const TMap<int32, FBulletInfo>& GetBulletMap() const;
    TMap<int32, FBulletInfo>& GetMutableBulletMap();

    void MarkNeedDestroy(int32 BulletId);
    const TSet<int32>& GetNeedDestroyBullets() const;

    void RemoveBullet(int32 BulletId);
    void Clear();

private:
    UPROPERTY()
    TMap<int32, FBulletInfo> BulletMap;

    TMap<int32, TSet<int32>> AttackerBullets;

    UPROPERTY()
    TSet<int32> NeedDestroyBullets;

    int32 NextBulletId = 1;
};
