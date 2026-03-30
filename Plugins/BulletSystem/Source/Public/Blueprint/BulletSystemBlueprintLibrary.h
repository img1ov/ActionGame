#pragma once

// BulletSystem: BulletBlueprintLibrary.h
// Blueprint-facing helpers.

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "BulletSystemTypes.h"
#include "Model/BulletInfo.h"
#include "BulletSystemBlueprintLibrary.generated.h"

class AActor;

UCLASS()
class BULLETSYSTEM_API UBulletSystemBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "BulletSystem")
    static int32 SpawnBullet(AActor* SourceActor, FName BulletId, const FBulletInitParams& InitParams);

    // Resolve a runtime bullet instance by InstanceKey from the owner's BulletSystemComponent registry.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|InstanceRegistry")
    static int32 GetInstanceIdByKey(AActor* SourceActor, FName InstanceKey);

    // Consume (remove) the oldest valid instance id for a key from the owner's registry.
    // Useful for overlapping notify windows where End should destroy the earliest spawned instance for that key.
    UFUNCTION(BlueprintCallable, Category = "BulletSystem|InstanceRegistry")
    static int32 ConsumeOldestInstanceIdByKey(AActor* SourceActor, FName InstanceKey);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool DestroyBullet(const UObject* WorldContextObject, int32 InstanceId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool IsBulletValid(const UObject* WorldContextObject, int32 InstanceId);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static bool SetBulletCollisionEnabled(const UObject* WorldContextObject, int32 InstanceId, bool bEnabled, bool bClearOverlaps);

    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static int32 ProcessManualHits(const UObject* WorldContextObject, int32 InstanceId, bool bApplyCollisionResponse);

    // Collision read-side helpers (runtime access from FBulletInfo).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Collision")
    static TArray<AActor*> GetBulletHitActors(const FBulletInfo& BulletInfo);

    // Returns actors that were accepted in the most recent hit batch.
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Collision")
    static TArray<AActor*> GetBulletHitActorsAtLastHitTime(const FBulletInfo& BulletInfo, float TimeTolerance = 0.001f);

    // Hard reset for iteration/debugging:
    // - clears the current world's bullet pools + live bullets
    // - clears config caches (and optionally rebuilds the runtime table)
    UFUNCTION(BlueprintCallable, Category = "BulletSystem", meta = (WorldContext = "WorldContextObject"))
    static void ClearBulletSystemRuntime(const UObject* WorldContextObject, bool bRebuildRuntimeTable);

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

    // Pure convenience: replace InitParams.Payload and return the updated struct (useful for Blueprint chaining).
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|Payload")
    static FBulletInitParams SetBulletPayload(FBulletInitParams InitParams, const FBulletPayload& Payload);

    // InitParams helpers.
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

	// HitReact helpers (GAS GameplayEvent payload).
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|HitReact")
	static bool GetHitReactImpulseFromEventData(const FGameplayEventData& EventData, FHitReactImpulse& OutHitReactImpulse);

	// Converts the hit-react strength scale into the world-space velocity vector expected by AddMove.
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "BulletSystem|HitReact")
	static FVector GetHitReactAddMoveWorldVelocity(const FHitReactImpulse& HitReactImpulse);
};
