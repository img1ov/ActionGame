#pragma once

// BulletSystem: BulletBlueprintLibrary.h
// Blueprint-facing helpers.

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BulletSystemTypes.h"
#include "Model/BulletInfo.h"
#include "BulletBlueprintLibrary.generated.h"

class UBulletConfig;

UCLASS()
class BULLETGAME_API UBulletBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static int32 SpawnBullet(const UObject* WorldContextObject, UBulletConfig* ConfigAsset, FName BulletID, const FBulletInitParams& InitParams);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool DestroyBullet(const UObject* WorldContextObject, int32 BulletId, EBulletDestroyReason Reason, bool bSpawnChildren);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool IsBulletValid(const UObject* WorldContextObject, int32 BulletId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool SetBulletCollisionEnabled(const UObject* WorldContextObject, int32 BulletId, bool bEnabled, bool bClearOverlaps, bool bResetHitActors);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool ResetBulletHitActors(const UObject* WorldContextObject, int32 BulletId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static int32 ProcessManualHits(const UObject* WorldContextObject, int32 BulletId, bool bResetHitActorsBefore, bool bApplyCollisionResponse);

    // Payload (SetByCaller) helpers.
    // Write-side: only intended to be used before SpawnBullet (InitParams will be copied into runtime BulletInfo).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static const FBulletPayload& SetPayloadSetByCallerMagnitudeByName(UPARAM(ref) FBulletPayload& Payload, FName DataName, float Magnitude);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static const FBulletPayload& SetPayloadSetByCallerMagnitudeByTag(UPARAM(ref) FBulletPayload& Payload, FGameplayTag DataTag, float Magnitude);

    // Read-side: runtime access from BulletInfo.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByName(const FBulletInfo& BulletInfo, FName DataName, float& OutMagnitude);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByTag(const FBulletInfo& BulletInfo, FGameplayTag DataTag, float& OutMagnitude);
};
