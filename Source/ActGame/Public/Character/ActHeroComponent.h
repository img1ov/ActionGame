// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "GameplayTagContainer.h"
#include "Input/ActInputConfig.h"

#include "ActHeroComponent.generated.h"

#define UE_API ACTGAME_API

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDataAvailableDelegate);

class UGameFrameworkComponentManager;
class UInputComponent;
class UInputMappingContext;
class UActInputConfig;
class UObject;
struct FActorInitStateChangedParams;
struct FFrame;
struct FGameplayTag;
struct FInputActionValue;

/**
 * Component that sets up input and camera handling for player controlled pawns (or bots that simulate players).
 * This depends on a PawnExtensionComponent to coordinate initialization.
 */
UCLASS(MinimalAPI, Blueprintable, Meta=(BlueprintSpawnableComponent))
class UActHeroComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	
	UE_API UActHeroComponent(const FObjectInitializer& ObjectInitializer);

	/** Returns the hero component if one exists on the specified actor. */
	UFUNCTION(BlueprintPure, Category="Act")
	static UActHeroComponent* FindHeroComponent(const AActor* Actor){ return (Actor ? Actor->FindComponentByClass<UActHeroComponent>() : nullptr);}

	/** True if this is controlled by a real player and has progressed far enough in initialization where additional input bindings can be added */
	UE_API bool IsReadyToBindInputs() const;
	
	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	UE_API virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	UE_API virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	UE_API virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	UE_API virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface
	
protected:

	UE_API virtual void OnRegister() override;
	UE_API virtual void BeginPlay() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UE_API void InitializePlayerInput(UInputComponent* PlayerInputComponent);
 
	UE_API void Input_AbilityInputTagPressed(FGameplayTag InputTag);
	UE_API void Input_AbilityInputTagReleased(FGameplayTag InputTag);

	UE_API void Input_Move(const FInputActionValue& InputActionValue);
	UE_API void Input_LookMouse(const FInputActionValue& InputActionValue);


public:

	/**
	 * Blocks locomotion input at the input-entry layer.
	 *
	 * Only AddMovementInput is blocked. Direction data still flows into the command analyzer so
	 * combat input matching continues to work during authored movement-lock windows.
	 */
	UFUNCTION(BlueprintCallable, Category = "Act|Input")
	UE_API void PushMovementInputBlock(UObject* Source);

	/** Removes one movement-input block source. */
	UFUNCTION(BlueprintCallable, Category = "Act|Input")
	UE_API void PopMovementInputBlock(UObject* Source);

	/** Clears all movement-input block sources. */
	UFUNCTION(BlueprintCallable, Category = "Act|Input")
	UE_API void ClearMovementInputBlocks();

	/** Clears the current authored locomotion input and optionally suppresses the next input sample. */
	UFUNCTION(BlueprintCallable, Category = "Act|Input")
	UE_API void FlushMovementInput(bool bSuppressNextSample = true);

	/** True if any valid movement-input block source is active. */
	UFUNCTION(BlueprintPure, Category = "Act|Input")
	UE_API bool IsMovementInputBlocked() const;

	/**
	 * Returns the latest authored move input in input-action space.
	 *
	 * Semantics:
	 * - X = right / left input
	 * - Y = forward / backward input
	 *
	 * This is raw local intent captured at the HeroComponent input-entry layer.
	 * It is intentionally independent from:
	 * - actual movement result
	 * - acceleration / velocity
	 * - movement-input blocking
	 *
	 * That makes it suitable for gameplay/animation code that needs the player's latest
	 * directional intent even while locomotion is temporarily blocked.
	 */
	UFUNCTION(BlueprintPure, Category = "Act|Input")
	UE_API FVector2D GetMoveInputVector() const { return MoveInputVector; }

	/**
	 * Converts the latest move input intent into a world-space direction using the owning actor's
	 * current forward/right basis.
	 *
	 * Returns ZeroVector when there is no meaningful move input or the owning pawn is unavailable.
	 */
	UFUNCTION(BlueprintPure, Category = "Act|Input")
	UE_API FVector GetMoveInputDirectionFromActor() const;

	/**
	 * Converts the latest move input intent into a world-space direction using the controller yaw
	 * basis that the movement input action uses.
	 *
	 * Returns ZeroVector when there is no meaningful move input, the owning pawn is unavailable,
	 * or no controller is present.
	 */
	UFUNCTION(BlueprintPure, Category = "Act|Input")
	UE_API FVector GetMoveInputDirectionFromController() const;

	/** Returns the active source ref-counts; empty means locomotion input is currently allowed. */
	const TMap<TWeakObjectPtr<UObject>, int32>& GetMovementInputBlockSources() const { return MovementInputBlockSources; }

public:
	
	/** The name of the extension event sent via UGameFrameworkComponentManager when ability inputs are ready to bind */
	static UE_API const FName NAME_BindInputsNow;

	/** The name of this component-implemented feature */
	static UE_API const FName NAME_ActorFeatureName;

	UPROPERTY(BlueprintAssignable)
	FOnDataAvailableDelegate OnDataAvailable;

protected:
	
	UPROPERTY(EditAnywhere, Category = "Act|Input")
	TObjectPtr<UInputMappingContext> DefaultInputMapping;

	/** True when player input bindings have been applied, will never be true for non - players */
	bool bReadyToBindInputs;

	/**
	 * Source-tracked movement input blocks.
	 *
	 * We use ref-counting instead of a plain set because the same notify/source may legitimately
	 * re-enter before the previous scope ended (section jumps, montage refresh, overlap windows).
	 * Input stays blocked until the last matching Pop is received.
	 */
	mutable TMap<TWeakObjectPtr<UObject>, int32> MovementInputBlockSources;

	/**
	 * Latest raw move input captured from the move action.
	 *
	 * This stores player intent, not movement result. It is updated on both Triggered and Completed
	 * input events so callers can safely treat ZeroVector as "no current move input".
	 */
	FVector2D MoveInputVector = FVector2D::ZeroVector;

private:

	void PruneMovementInputBlockSources() const;
	void FlushPendingMovementInput(bool bSuppressNextFrame);

	mutable bool bSuppressMovementInputUntilNextSample = false;
};

#undef UE_API
