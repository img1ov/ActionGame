#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"

#include "ActMontageRootMotionSource.generated.h"

class UAnimMontage;
class FReferenceCollector;

UENUM(BlueprintType)
enum class EActMontageRootMotionRangeMode : uint8
{
	CurrentSection,
	ThroughSection,
	EntireMontage,
};

UENUM(BlueprintType)
enum class EActMontageMotionExecutionMode : uint8
{
	AddMove,
	RootMotionSource
};

USTRUCT(BlueprintType)
struct FActMontageRootMotionSourceSettings
{
	GENERATED_BODY()

	/**
	 * Minimal GA-side bridge into the movement framework.
	 * Abilities only describe how montage motion should be extracted;
	 * the actual movement authority lives in CharacterMovement/AddMove.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion")
	bool bEnabled = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	EActMontageRootMotionRangeMode RangeMode = EActMontageRootMotionRangeMode::CurrentSection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	EActMontageMotionExecutionMode ExecutionMode = EActMontageMotionExecutionMode::AddMove;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled && RangeMode == EActMontageRootMotionRangeMode::ThroughSection"))
	FName EndSectionName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	bool bApplyRotation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	bool bIgnoreZAccumulate = true;

	/**
	 * If true, zero out character movement velocity when the first extracted montage motion segment starts.
	 * This is primarily intended for attack startup so locomotion momentum does not bleed into authored attack motion.
	 * Section refreshes within the same task do not retrigger the brake.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	bool bBrakeMovementOnStart = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled", ClampMin = "0"))
	int32 Priority = 1000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	ERootMotionFinishVelocityMode FinishVelocityMode = ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled"))
	FVector FinishSetVelocity = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RootMotion", meta = (EditCondition = "bEnabled", ClampMin = "0"))
	float FinishClampVelocity = 0.0f;
};

USTRUCT()
struct FActRootMotionSource_Montage : public FRootMotionSource
{
	GENERATED_USTRUCT_BODY()

	FActRootMotionSource_Montage();

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY()
	float StartTrackPosition;

	UPROPERTY()
	float EndTrackPosition;

	UPROPERTY()
	bool bApplyRotation;

	UPROPERTY()
	bool bIgnoreZAccumulate;

	virtual FRootMotionSource* Clone() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;
	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent) override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	float MapTimeToTrackPosition(float TimeSeconds) const;
};

template<>
struct TStructOpsTypeTraits<FActRootMotionSource_Montage> : public TStructOpsTypeTraitsBase2<FActRootMotionSource_Montage>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
