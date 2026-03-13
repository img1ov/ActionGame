// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Engine/GameInstance.h"

#include "ActGameInstance.generated.h"

#define UE_API ACTGAME_API

UCLASS(MinimalAPI, Config = Game)
class UActGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	UE_API UActGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:

	UE_API virtual void Init() override;
	
};

#undef UE_API
