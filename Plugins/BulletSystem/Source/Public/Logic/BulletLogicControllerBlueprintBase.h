#pragma once

// BulletSystem: BulletLogicControllerBlueprintBase.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "BulletLogicController.h"
#include "Engine/EngineTypes.h"

#include "BulletLogicControllerBlueprintBase.generated.h"

UCLASS(Blueprintable)
class BULLETSYSTEM_API UBulletLogicControllerBlueprintBase : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
    virtual void OnDestroy(FBulletInfo& BulletInfo) override;
    virtual void OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
    virtual void OnSupport(FBulletInfo& BulletInfo) override;
    virtual void OnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId) override;
    virtual void Tick(FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual bool ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual void OnPreMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual void OnPostMove(FBulletInfo& BulletInfo, float DeltaSeconds) override;
    virtual void OnPreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits) override;
    virtual void OnPostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits) override;
    virtual bool FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;

protected:
    // NOTE: Blueprint "Event" overrides cannot reliably write back to non-const ref params.
    // Use const-ref here to avoid misleading by-ref behavior and Blueprint compile warnings.
    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnBegin"))
    void K2_OnBegin(const FBulletInfo& BulletInfo);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnHit"))
    void K2_OnHit(const FBulletInfo& BulletInfo, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnDestroy"))
    void K2_OnDestroy(const FBulletInfo& BulletInfo);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnRebound"))
    void K2_OnRebound(const FBulletInfo& BulletInfo, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnSupport"))
    void K2_OnSupport(const FBulletInfo& BulletInfo);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnHitBullet"))
    void K2_OnHitBullet(const FBulletInfo& BulletInfo, int32 OtherBulletId);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "Tick"))
    void K2_Tick(const FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "ReplaceMove"))
    bool K2_ReplaceMove(UPARAM(ref) FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPreMove"))
    void K2_OnPreMove(const FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPostMove"))
    void K2_OnPostMove(const FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPreCollision"))
    void K2_OnPreCollision(const FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPostCollision"))
    void K2_OnPostCollision(const FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "FilterHit"))
    bool K2_FilterHit(UPARAM(ref) FBulletInfo& BulletInfo, const FHitResult& Hit);
};
