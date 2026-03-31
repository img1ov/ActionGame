#pragma once

#include "CoreMinimal.h"

#include "ActCharacterMovementTypes.generated.h"

USTRUCT(BlueprintType)
struct FActMovementStateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float MaxWalkSpeed = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float MaxAcceleration = 2048.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float BrakingDecelerationWalking = 2048.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	float GroundFriction = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	FRotator RotationRate = FRotator(0.0f, 720.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	bool bOrientRotationToMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement State")
	bool bUseControllerDesiredRotation = false;
};
