// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Teams/ActTeamInfoBase.h"

#include "ActTeamPrivateInfo.generated.h"

class UObject;

UCLASS()
class ACTGAME_API AActTeamPrivateInfo : public AActTeamInfoBase
{
	GENERATED_BODY()
	
public:
	AActTeamPrivateInfo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
