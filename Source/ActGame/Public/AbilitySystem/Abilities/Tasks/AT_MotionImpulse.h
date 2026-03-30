#pragma once

#include "Abilities/Tasks/AbilityTask.h"
#include "Character/ActCharacterMovementTypes.h"

#include "AT_MotionImpulse.generated.h"

class UActCharacterMovementComponent;
class UCurveFloat;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMotionImpulseDelegate);

UCLASS()
class ACTGAME_API UAT_MotionImpulse : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAT_MotionImpulse(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FMotionImpulseDelegate OnFinish;

	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (DisplayName = "MotionImpulse", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAT_MotionImpulse* MotionImpulse(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		EActMotionBasisMode BasisMode,
		FVector Direction,
		float Strength,
		float Duration,
		EActMotionRotationSourceType RotationSourceType = EActMotionRotationSourceType::None,
		AActor* RotationTarget = nullptr,
		EActMotionRotationActorMode RotationActorMode = EActMotionRotationActorMode::FaceTarget,
		UCurveFloat* StrengthOverTime = nullptr,
		bool bUseOppositeMovementDirection = false,
		bool bAdditiveRotation = false,
		EActMotionRotationPriority RotationPriority = EActMotionRotationPriority::Attack);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void ExternalCancel() override;
	virtual void OnDestroy(bool bInOwnerFinished) override;
	virtual FString GetDebugString() const override;

private:
	UActCharacterMovementComponent* GetActMovementComponent() const;
	bool ApplyImpulseMotion();
	bool HasInfiniteDuration() const;
	void OnTimeFinish();
	FActMotionRotationParams BuildRotationParams() const;

private:
	UPROPERTY()
	EActMotionBasisMode BasisMode = EActMotionBasisMode::World;

	UPROPERTY()
	FVector Direction = FVector::ForwardVector;

	UPROPERTY()
	float Strength = 0.0f;

	UPROPERTY()
	float Duration = 0.0f;

	UPROPERTY()
	EActMotionRotationSourceType RotationSourceType = EActMotionRotationSourceType::None;

	UPROPERTY()
	TObjectPtr<AActor> RotationTarget = nullptr;

	UPROPERTY()
	EActMotionRotationActorMode RotationActorMode = EActMotionRotationActorMode::FaceTarget;

	UPROPERTY()
	TObjectPtr<UCurveFloat> StrengthOverTime = nullptr;

	UPROPERTY()
	bool bUseOppositeMovementDirection = false;

	UPROPERTY()
	bool bAdditiveRotation = false;

	UPROPERTY()
	EActMotionRotationPriority RotationPriority = EActMotionRotationPriority::Attack;

	bool bFinishWhenMotionCompletes = false;
	int32 MotionHandle = INDEX_NONE;
	int32 MotionSyncId = INDEX_NONE;
	FTimerHandle FinishTimerHandle;
};
