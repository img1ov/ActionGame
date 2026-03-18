// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ActVerbMessageHelpers.generated.h"

#define UE_API ACTGAME_API

struct FGameplayCueParameters;
struct FActVerbMessage;

class APlayerController;
class APlayerState;
class UObject;
struct FFrame;


UCLASS(MinimalAPI)
class UActVerbMessageHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Act")
	static UE_API APlayerState* GetPlayerStateFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Act")
	static UE_API APlayerController* GetPlayerControllerFromObject(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Act")
	static UE_API FGameplayCueParameters VerbMessageToCueParameters(const FActVerbMessage& Message);

	UFUNCTION(BlueprintCallable, Category = "Act")
	static UE_API FActVerbMessage CueParametersToVerbMessage(const FGameplayCueParameters& Params);
};

#undef UE_API
