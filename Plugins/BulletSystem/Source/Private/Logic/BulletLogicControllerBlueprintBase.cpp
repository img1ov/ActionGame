// BulletSystem: BulletLogicControllerBlueprintBase.cpp
// Blueprint extension hooks for LogicControllers.

#include "Logic/BulletLogicControllerBlueprintBase.h"

#include "Model/BulletInfo.h"

void UBulletLogicControllerBlueprintBase::OnBegin(FBulletInfo& BulletInfo)
{
    K2_OnBegin(BulletInfo);
}

void UBulletLogicControllerBlueprintBase::OnHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    K2_OnHit(BulletInfo, Hit);
}

void UBulletLogicControllerBlueprintBase::OnDestroy(FBulletInfo& BulletInfo)
{
    K2_OnDestroy(BulletInfo);
}

void UBulletLogicControllerBlueprintBase::OnRebound(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    K2_OnRebound(BulletInfo, Hit);
}

void UBulletLogicControllerBlueprintBase::OnSupport(FBulletInfo& BulletInfo)
{
    K2_OnSupport(BulletInfo);
}

void UBulletLogicControllerBlueprintBase::OnHitBullet(FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    K2_OnHitBullet(BulletInfo, OtherBulletId);
}

void UBulletLogicControllerBlueprintBase::Tick(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_Tick(BulletInfo, DeltaSeconds);
}

bool UBulletLogicControllerBlueprintBase::ReplaceMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    return K2_ReplaceMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicControllerBlueprintBase::OnPreMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_OnPreMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicControllerBlueprintBase::OnPostMove(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    K2_OnPostMove(BulletInfo, DeltaSeconds);
}

void UBulletLogicControllerBlueprintBase::OnPreCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    K2_OnPreCollision(BulletInfo, Hits);
}

void UBulletLogicControllerBlueprintBase::OnPostCollision(FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    K2_OnPostCollision(BulletInfo, Hits);
}

bool UBulletLogicControllerBlueprintBase::FilterHit(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    return K2_FilterHit(BulletInfo, Hit);
}

void UBulletLogicControllerBlueprintBase::K2_OnBegin_Implementation(const FBulletInfo& BulletInfo)
{
    (void)BulletInfo;
}

void UBulletLogicControllerBlueprintBase::K2_OnHit_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
}

void UBulletLogicControllerBlueprintBase::K2_OnDestroy_Implementation(const FBulletInfo& BulletInfo)
{
    (void)BulletInfo;
}

void UBulletLogicControllerBlueprintBase::K2_OnRebound_Implementation(const FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
}

void UBulletLogicControllerBlueprintBase::K2_OnSupport_Implementation(const FBulletInfo& BulletInfo)
{
    (void)BulletInfo;
}

void UBulletLogicControllerBlueprintBase::K2_OnHitBullet_Implementation(const FBulletInfo& BulletInfo, int32 OtherBulletId)
{
    (void)BulletInfo;
    (void)OtherBulletId;
}

void UBulletLogicControllerBlueprintBase::K2_Tick_Implementation(const FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

bool UBulletLogicControllerBlueprintBase::K2_ReplaceMove_Implementation(FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
    return false;
}

void UBulletLogicControllerBlueprintBase::K2_OnPreMove_Implementation(const FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

void UBulletLogicControllerBlueprintBase::K2_OnPostMove_Implementation(const FBulletInfo& BulletInfo, float DeltaSeconds)
{
    (void)BulletInfo;
    (void)DeltaSeconds;
}

void UBulletLogicControllerBlueprintBase::K2_OnPreCollision_Implementation(const FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    (void)BulletInfo;
    (void)Hits;
}

void UBulletLogicControllerBlueprintBase::K2_OnPostCollision_Implementation(const FBulletInfo& BulletInfo, const TArray<FHitResult>& Hits)
{
    (void)BulletInfo;
    (void)Hits;
}

bool UBulletLogicControllerBlueprintBase::K2_FilterHit_Implementation(FBulletInfo& BulletInfo, const FHitResult& Hit)
{
    (void)BulletInfo;
    (void)Hit;
    return true;
}
