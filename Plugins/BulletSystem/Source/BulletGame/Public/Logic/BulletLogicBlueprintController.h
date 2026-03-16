#pragma once

// BulletSystem: BulletLogicBlueprintController.h
// Logic data assets and controllers.

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Logic/BulletLogicControllers.h"
#include "BulletLogicBlueprintController.generated.h"

UCLASS(Blueprintable)
class BULLETGAME_API UBulletLogicBlueprintController : public UBulletLogicController
{
    GENERATED_BODY()

public:
    virtual void OnBegin(FBulletInfo& BulletInfo) override;
    virtual void OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit) override;
    virtual void OnDestroy(FBulletInfo& BulletInfo, EBulletDestroyReason Reason) override;
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
    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnBegin"))
    void K2_OnBegin(UPARAM(ref) FBulletInfo& BulletInfo);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnHit"))
    void K2_OnHit(UPARAM(ref) FBulletInfo& BulletInfo, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnDestroy"))
    void K2_OnDestroy(UPARAM(ref) FBulletInfo& BulletInfo, EBulletDestroyReason Reason);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnRebound"))
    void K2_OnRebound(UPARAM(ref) FBulletInfo& BulletInfo, const FHitResult& Hit);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnSupport"))
    void K2_OnSupport(UPARAM(ref) FBulletInfo& BulletInfo);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnHitBullet"))
    void K2_OnHitBullet(UPARAM(ref) FBulletInfo& BulletInfo, int32 OtherBulletId);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "Tick"))
    void K2_Tick(UPARAM(ref) FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "ReplaceMove"))
    bool K2_ReplaceMove(UPARAM(ref) FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPreMove"))
    void K2_OnPreMove(UPARAM(ref) FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPostMove"))
    void K2_OnPostMove(UPARAM(ref) FBulletInfo& BulletInfo, float DeltaSeconds);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPreCollision"))
    void K2_OnPreCollision(UPARAM(ref) FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "OnPostCollision"))
    void K2_OnPostCollision(UPARAM(ref) FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits);

    UFUNCTION(BlueprintNativeEvent, Category = "BulletSystem|Logic", meta = (DisplayName = "FilterHit"))
    bool K2_FilterHit(UPARAM(ref) FBulletInfo& BulletInfo, const FHitResult& Hit);
};

