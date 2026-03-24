// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ActTeamInfoBase.h"

#include "ActTeamPublicInfo.generated.h"

class UActTeamCreationComponent;
class UActTeamDisplayAsset;
class UObject;
struct FFrame;

UCLASS()
class AActTeamPublicInfo : public AActTeamInfoBase
{
	GENERATED_BODY()

	friend UActTeamCreationComponent;

public:
	AActTeamPublicInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UActTeamDisplayAsset* GetTeamDisplayAsset() const { return TeamDisplayAsset; }

private:
	UFUNCTION()
	void OnRep_TeamDisplayAsset();

	void SetTeamDisplayAsset(TObjectPtr<UActTeamDisplayAsset> NewDisplayAsset);

private:
	UPROPERTY(ReplicatedUsing=OnRep_TeamDisplayAsset)
	TObjectPtr<UActTeamDisplayAsset> TeamDisplayAsset;
};
