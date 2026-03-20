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

    // Write-side convenience: mutate InitParams in-place with exec pins (recommended in Blueprints).
    UFUNCTION(BlueprintCallable, Category = "BulletSystem|Payload")
    static void SetInitParamsSetByCallerMagnitudeByName(UPARAM(ref) FBulletInitParams& InitParams, FName DataName, float Magnitude);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem|Payload")
    static void SetInitParamsSetByCallerMagnitudeByTag(UPARAM(ref) FBulletInitParams& InitParams, FGameplayTag DataTag, float Magnitude);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem|Payload")
    static void ClearPayload(UPARAM(ref) FBulletPayload& Payload);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem|Payload")
    static void ClearInitParamsPayload(UPARAM(ref) FBulletInitParams& InitParams);

    // Read-side: runtime access from BulletInfo.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByName(const FBulletInfo& BulletInfo, FName DataName, float& OutMagnitude);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByTag(const FBulletInfo& BulletInfo, FGameplayTag DataTag, float& OutMagnitude);

    // Read-side: access directly from a payload (useful pre-spawn in Blueprints without exposing the maps).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByNameFromPayload(const FBulletPayload& Payload, FName DataName, float& OutMagnitude);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static bool GetPayloadSetByCallerMagnitudeByTagFromPayload(const FBulletPayload& Payload, FGameplayTag DataTag, float& OutMagnitude);
};
