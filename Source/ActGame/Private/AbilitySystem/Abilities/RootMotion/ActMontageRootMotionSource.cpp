#include "AbilitySystem/Abilities/RootMotion/ActMontageRootMotionSource.h"

#include "Animation/AnimMontage.h"

FActRootMotionSource_Montage::FActRootMotionSource_Montage()
	: Montage(nullptr)
	, StartTrackPosition(0.0f)
	, EndTrackPosition(0.0f)
	, bApplyRotation(true)
	, bIgnoreZAccumulate(true)
{
	AccumulateMode = ERootMotionAccumulateMode::Override;
	bInLocalSpace = true;
}

FRootMotionSource* FActRootMotionSource_Montage::Clone() const
{
	return new FActRootMotionSource_Montage(*this);
}

bool FActRootMotionSource_Montage::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	const FActRootMotionSource_Montage* OtherCast = static_cast<const FActRootMotionSource_Montage*>(Other);
	return Montage == OtherCast->Montage
		&& FMath::IsNearlyEqual(StartTrackPosition, OtherCast->StartTrackPosition, UE_SMALL_NUMBER)
		&& FMath::IsNearlyEqual(EndTrackPosition, OtherCast->EndTrackPosition, UE_SMALL_NUMBER)
		&& bApplyRotation == OtherCast->bApplyRotation
		&& bIgnoreZAccumulate == OtherCast->bIgnoreZAccumulate;
}

bool FActRootMotionSource_Montage::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	return FRootMotionSource::MatchesAndHasSameState(Other);
}

bool FActRootMotionSource_Montage::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup)
{
	return FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup);
}

void FActRootMotionSource_Montage::PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	if (!Montage || Duration <= UE_SMALL_NUMBER || SimulationTime <= UE_SMALL_NUMBER)
	{
		SetTime(GetTime() + SimulationTime);
		return;
	}

	const float PreviousTrackPosition = MapTimeToTrackPosition(GetTime());
	const float NewTime = FMath::Clamp(GetTime() + SimulationTime, 0.0f, Duration);
	const float CurrentTrackPosition = MapTimeToTrackPosition(NewTime);

	FTransform DeltaTransform = Montage->ExtractRootMotionFromTrackRange(PreviousTrackPosition, CurrentTrackPosition, FAnimExtractContext());
	if (!bApplyRotation)
	{
		DeltaTransform.SetRotation(FQuat::Identity);
	}

	if (bIgnoreZAccumulate)
	{
		Settings.SetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	}
	else
	{
		Settings.UnSetFlag(ERootMotionSourceSettingsFlags::IgnoreZAccumulate);
	}

	RootMotionParams.Set(DeltaTransform);
	SetTime(NewTime);
}

bool FActRootMotionSource_Montage::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	Ar << Montage;
	Ar << StartTrackPosition;
	Ar << EndTrackPosition;
	Ar << bApplyRotation;
	Ar << bIgnoreZAccumulate;

	bOutSuccess = true;
	return true;
}

UScriptStruct* FActRootMotionSource_Montage::GetScriptStruct() const
{
	return FActRootMotionSource_Montage::StaticStruct();
}

FString FActRootMotionSource_Montage::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FActRootMotionSource_Montage %s Montage=%s Start=%.3f End=%.3f"),
		LocalID,
		*InstanceName.GetPlainNameString(),
		*GetNameSafe(Montage),
		StartTrackPosition,
		EndTrackPosition);
}

void FActRootMotionSource_Montage::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Montage);
	FRootMotionSource::AddReferencedObjects(Collector);
}

float FActRootMotionSource_Montage::MapTimeToTrackPosition(float TimeSeconds) const
{
	if (Duration <= UE_SMALL_NUMBER)
	{
		return EndTrackPosition;
	}

	const float Alpha = FMath::Clamp(TimeSeconds / Duration, 0.0f, 1.0f);
	return FMath::Lerp(StartTrackPosition, EndTrackPosition, Alpha);
}
