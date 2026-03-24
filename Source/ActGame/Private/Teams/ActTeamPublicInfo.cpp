// Copyright Epic Games, Inc. All Rights Reserved.

#include "Teams/ActTeamPublicInfo.h"

#include "Net/UnrealNetwork.h"
#include "Teams/ActTeamInfoBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActTeamPublicInfo)

class FLifetimeProperty;

AActTeamPublicInfo::AActTeamPublicInfo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AActTeamPublicInfo::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, TeamDisplayAsset, COND_InitialOnly);
}

void AActTeamPublicInfo::SetTeamDisplayAsset(TObjectPtr<UActTeamDisplayAsset> NewDisplayAsset)
{
	check(HasAuthority());
	check(TeamDisplayAsset == nullptr);

	TeamDisplayAsset = NewDisplayAsset;

	TryRegisterWithTeamSubsystem();
}

void AActTeamPublicInfo::OnRep_TeamDisplayAsset()
{
	TryRegisterWithTeamSubsystem();
}

