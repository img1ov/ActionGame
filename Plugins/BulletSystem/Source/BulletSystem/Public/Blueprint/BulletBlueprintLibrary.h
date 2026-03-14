#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BulletSystemTypes.h"
#include "BulletBlueprintLibrary.generated.h"

class UBulletConfig;

UCLASS()
class BULLETSYSTEM_API UBulletBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static int32 CreateBullet(const UObject* WorldContextObject, UBulletConfig* ConfigAsset, FName RowName, const FBulletInitParams& InitParams);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool DestroyBullet(const UObject* WorldContextObject, int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool IsBulletValid(const UObject* WorldContextObject, int32 BulletId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool SetBulletCollisionEnabled(const UObject* WorldContextObject, int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool ResetBulletHitActors(const UObject* WorldContextObject, int32 BulletId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static int32 ApplyDamageToOverlaps(const UObject* WorldContextObject, int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse);
};
