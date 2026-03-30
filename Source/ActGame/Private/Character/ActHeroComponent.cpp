// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ActHeroComponent.h"

#include "ActGameplayTags.h"
#include "ActLogChannels.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Character/ActBattleComponent.h"
#include "Character/ActCharacterMovementComponent.h"
#include "Character/ActPawnData.h"
#include "Character/ActPawnExtensionComponent.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Input/ActInputComponent.h"
#include "Input/ActInputCommandConfig.h"
#include "BulletSystemComponent.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Player/ActLocalPlayer.h"
#include "Player/ActPlayerController.h"
#include "Player/ActPlayerState.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
const FGameplayTag InputTag_StrafeMove = ActGameplayTags::FindTagByString(TEXT("InputTag.StrafeMove"), false);

EInputDirection QuantizeLocalDirection(const FVector2D& LocalDirection)
{
	// Quantize analog stick / WASD input into 8 directions plus Neutral for command matching.
	static constexpr float NeutralDeadZone = 0.35f;
	static constexpr float AxisThreshold = 0.38268343f; // sin(22.5 deg)

	if (LocalDirection.SizeSquared() < FMath::Square(NeutralDeadZone))
	{
		return EInputDirection::Neutral;
	}

	const FVector2D Normalized = LocalDirection.GetSafeNormal();
	const float X = Normalized.X; // right
	const float Y = Normalized.Y; // forward

	const bool bForward = Y > AxisThreshold;
	const bool bBackward = Y < -AxisThreshold;
	const bool bRight = X > AxisThreshold;
	const bool bLeft = X < -AxisThreshold;

	if (bForward && bRight) { return EInputDirection::ForwardRight; }
	if (bForward && bLeft) { return EInputDirection::ForwardLeft; }
	if (bBackward && bRight) { return EInputDirection::BackwardRight; }
	if (bBackward && bLeft) { return EInputDirection::BackwardLeft; }
	if (bForward) { return EInputDirection::Forward; }
	if (bBackward) { return EInputDirection::Backward; }
	if (bRight) { return EInputDirection::Right; }
	if (bLeft) { return EInputDirection::Left; }

	// Near axis boundaries fallback by dominant component.
	return (FMath::Abs(Y) >= FMath::Abs(X))
		? (Y >= 0.0f ? EInputDirection::Forward : EInputDirection::Backward)
		: (X >= 0.0f ? EInputDirection::Right : EInputDirection::Left);
}
}

const FName UActHeroComponent::NAME_BindInputsNow("BindInputsNow");
const FName UActHeroComponent::NAME_ActorFeatureName("Battle");

UActHeroComponent::UActHeroComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//TODO CameraMode
	bReadyToBindInputs = false;
}

bool UActHeroComponent::IsReadyToBindInputs() const
{
	return bReadyToBindInputs;
}

bool UActHeroComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();

	if (!CurrentState.IsValid() && DesiredState == ActGameplayTags::InitState_Spawned)
	{
		// As long as we have a real pawn, let us transition
		if (Pawn)
		{
			return true;
		}
	}
	else if (CurrentState == ActGameplayTags::InitState_Spawned && DesiredState == ActGameplayTags::InitState_DataAvailable)
	{
		// The player state is required.
		if (!GetPlayerState<AActPlayerState>())
		{
			return false;
		}

		// If we're authority or autonomous, we need to wait for a controller with registered ownership of the player state.
		if (Pawn->GetLocalRole() != ROLE_SimulatedProxy)
		{
			AController* Controller = GetController<AController>();

			const bool bHasControllerPairedWithPS = (Controller != nullptr) && \
				(Controller->PlayerState != nullptr) && \
				(Controller->PlayerState->GetOwner() == Controller);

			if (!bHasControllerPairedWithPS)
			{
				return false;
			}
		}

		const bool bIsLocallyControlled = Pawn->IsLocallyControlled();
		const bool bIsBot = Pawn->IsBotControlled();

		if (bIsLocallyControlled && !bIsBot)
		{
			AActPlayerController* ActPC = GetController<AActPlayerController>();

			// The input component and local player is required when locally controlled.
			if (!Pawn->InputComponent || !ActPC || !ActPC->GetLocalPlayer())
			{
				return false;
			}
		}

		return true;
	}
	else if (CurrentState == ActGameplayTags::InitState_DataAvailable && DesiredState == ActGameplayTags::InitState_DataInitialized)
	{
		// Wait for player state and extension component
		AActPlayerState* ActPS = GetPlayerState<AActPlayerState>();

		return ActPS && Manager->HasFeatureReachedInitState(Pawn, UActPawnExtensionComponent::NAME_ActorFeatureName, ActGameplayTags::InitState_DataInitialized);
	}
	else if (CurrentState == ActGameplayTags::InitState_DataInitialized && DesiredState == ActGameplayTags::InitState_GameplayReady)
	{
		// TODO add ability initialization checks?
		return true;
	}

	return false;
}

void UActHeroComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
	if (CurrentState == ActGameplayTags::InitState_DataAvailable && DesiredState == ActGameplayTags::InitState_DataInitialized)
	{
		APawn* Pawn = GetPawn<APawn>();
		AActPlayerState* ActPS = GetPlayerState<AActPlayerState>();
		if (!ensure(Pawn && ActPS))
		{
			return;
		}

		const UActPawnData* PawnData = nullptr;

		if (UActPawnExtensionComponent* PawnExtComp = UActPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			PawnData = PawnExtComp->GetPawnData<UActPawnData>();

			// The player state holds the persistent data for this player (state that persists across deaths and multiple pawns).
			// The ability system component and attribute sets live on the player state.
			PawnExtComp->InitializeAbilitySystem(ActPS->GetActAbilitySystemComponent(), ActPS);
		}

		// Forward PawnData bullet config into the pawn's BulletSystemComponent (local-only, non-replicated).
		// This keeps PawnExtensionComponent clean (system-level), while still ensuring each machine can initialize its
		// local BulletWorldSubsystem consistently (authority, autonomous, simulated proxies).
		if (PawnData && PawnData->BulletConfig)
		{
			if (UBulletSystemComponent* BulletComp = Pawn->FindComponentByClass<UBulletSystemComponent>())
			{
				BulletComp->SetBulletConfig(PawnData->BulletConfig);
			}
		}

		if (AActPlayerController* ActPC = GetController<AActPlayerController>())
		{
			if (Pawn->InputComponent != nullptr)
			{
				InitializePlayerInput(Pawn->InputComponent);
			}
		}

		OnDataAvailable.Broadcast();

		//TODO: CameraMode
		/*
		// Hook up the delegate for all pawns, in case we spectate later
		if (PawnData)
		{
			if (UActCameraComponent* CameraComponent = UActCameraComponent::FindCameraComponent(Pawn))
			{
				CameraComponent->DetermineCameraModeDelegate.BindUObject(this, &ThisClass::DetermineCameraMode);
			}
		}*/
	}
}

void UActHeroComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName == UActPawnExtensionComponent::NAME_ActorFeatureName)
	{
		if (Params.FeatureState == ActGameplayTags::InitState_DataInitialized)
		{
			// If the extension component says all other components are initialized, try to progress to next state
			CheckDefaultInitialization();
		}
	}
}

void UActHeroComponent::CheckDefaultInitialization()
{
	static const TArray<FGameplayTag> StateChain = { ActGameplayTags::InitState_Spawned, ActGameplayTags::InitState_DataAvailable, ActGameplayTags::InitState_DataInitialized, ActGameplayTags::InitState_GameplayReady };

	// This will try to progress from spawned (which is only set in BeginPlay) through the data initialization stages until it gets to gameplay ready
	ContinueInitStateChain(StateChain);
}

void UActHeroComponent::OnRegister()
{
	Super::OnRegister();

	if (!GetPawn<APawn>())
	{
		UE_LOG(LogAct, Error, TEXT("[UActBattleComponent::OnRegister] This component has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint."));

#if WITH_EDITOR
		if (GIsEditor)
		{
			static const FText Message = NSLOCTEXT("ActBattleComponent", "NotOnPawnError", "has been added to a blueprint whose base class is not a Pawn. To use this component, it MUST be placed on a Pawn Blueprint. This will cause a crash if you PIE!");
			static const FName BattleMessageLogName = TEXT("ActBattleComponent");
			
			FMessageLog(BattleMessageLogName).Error()
				->AddToken(FUObjectToken::Create(this, FText::FromString(GetNameSafe(this))))
				->AddToken(FTextToken::Create(Message));
				
			FMessageLog(BattleMessageLogName).Open();
		}
#endif
	}
	else
	{
		// Register with the init state system early, this will only work if this is a game world
		RegisterInitStateFeature();
	}
}

void UActHeroComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// Listen for when the pawn extension component changes init state
	BindOnActorInitStateChanged(UActPawnExtensionComponent::NAME_ActorFeatureName, FGameplayTag(), false);

	// Notifies that we are done spawning, then try the rest of initialization
	ensure(TryToChangeInitState(ActGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UActHeroComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearMovementInputBlocks();
	UnregisterInitStateFeature();
	
	Super::EndPlay(EndPlayReason);
}

void UActHeroComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	check(PC);
	
	const UActLocalPlayer* LP = Cast<UActLocalPlayer>(PC->GetLocalPlayer());
	check(LP);

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(InputSubsystem);

	// Clear existing mappings on re-init to avoid stale contexts after respawn/pawn swap.
	InputSubsystem->ClearAllMappings();
	
	if (const UActPawnExtensionComponent* PawnExtComp = UActPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
	{
		if (const UActPawnData* PawnData = PawnExtComp->GetPawnData())
		{
			if (AActPlayerController* ActPC = GetController<AActPlayerController>())
			{
				// Command definitions are runtime config from PawnData and are pushed into the controller analyzer.
				const TArray<FInputCommandDefinition> EmptyDefinitions;
				if (const UActInputCommandConfig* InputCommandConfig = PawnData->InputCommandConfig)
				{
					ActPC->ConfigureInputCommandDefinitions(InputCommandConfig->CommandDefinitions);
				}
				else
				{
					ActPC->ConfigureInputCommandDefinitions(EmptyDefinitions);
				}

			}

			if (const UActInputConfig* InputConfig = PawnData->InputConfig)
			{
				if (DefaultInputMapping != nullptr)
				{
					// Register in user settings so key rebinding can persist this mapping context.
					if (UEnhancedInputUserSettings* Settings = InputSubsystem->GetUserSettings())
					{
						Settings->RegisterInputMappingContext(DefaultInputMapping);
					}

					FModifyContextOptions Options = {};
					Options.bIgnoreAllPressedKeysUntilRelease = false;

					InputSubsystem->AddMappingContext(DefaultInputMapping, 0, Options);
				}

				// The Act Input Component has some additional functions to map Gameplay Tags to an Input Action.
				// If you want this functionality but still want to change your input component class, make it a subclass
				// of the UActInputComponent or modify this component accordingly.
				UActInputComponent* ActIC = Cast<UActInputComponent>(PlayerInputComponent);
				if (ensureMsgf(ActIC, TEXT("Unexpected Input Component class! The Gameplay Abilities will not be bound to their inputs. Change the input component to UActInputComponent or a subclass of it."))){
					// Add the key mappings that may have been set by the player
					ActIC->AddInputMappings(InputConfig, InputSubsystem);

					// Bind gameplay input tags through the configured press/release events on the input config.
					TArray<uint32> BindHandles;
					ActIC->BindAbilityActions(InputConfig, this, &ThisClass::Input_AbilityInputTagPressed, &ThisClass::Input_AbilityInputTagReleased, /*out*/BindHandles);
					
					// Native actions handle core locomotion/camera operations, not ability trigger flow.
					ActIC->BindNativeAction(InputConfig, ActGameplayTags::InputTag_Move, ETriggerEvent::Triggered, this, &ThisClass::Input_Move, false);
					ActIC->BindNativeAction(InputConfig, ActGameplayTags::InputTag_Move, ETriggerEvent::Completed, this, &ThisClass::Input_Move, false);
					ActIC->BindNativeAction(InputConfig, ActGameplayTags::InputTag_Look_Mouse, ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, false);
				}
			}
		}
	}

	if (ensure(!bReadyToBindInputs))
	{
		bReadyToBindInputs = true;
	}
 
	// Broadcast a unified hook so other components can bind additional inputs now.
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), NAME_BindInputsNow);
}

void UActHeroComponent::PushMovementInputBlock(UObject* Source)
{
	if (!Source)
	{
		return;
	}

	const bool bWasBlocked = IsMovementInputBlocked();
	TWeakObjectPtr<UObject> SourceKey(Source);
	int32& RefCount = MovementInputBlockSources.FindOrAdd(SourceKey);
	RefCount = FMath::Max(0, RefCount) + 1;

	if (!bWasBlocked)
	{
		FlushPendingMovementInput(false);
	}
}

void UActHeroComponent::PopMovementInputBlock(UObject* Source)
{
	if (!Source)
	{
		return;
	}

	const bool bWasBlocked = IsMovementInputBlocked();
	const TWeakObjectPtr<UObject> SourceKey(Source);
	if (int32* RefCount = MovementInputBlockSources.Find(SourceKey))
	{
		--(*RefCount);
		if (*RefCount <= 0)
		{
			MovementInputBlockSources.Remove(SourceKey);
		}
	}

	PruneMovementInputBlockSources();

	if (bWasBlocked && !IsMovementInputBlocked())
	{
		FlushPendingMovementInput(true);
	}
}

void UActHeroComponent::ClearMovementInputBlocks()
{
	MovementInputBlockSources.Reset();
	FlushPendingMovementInput(true);
}

bool UActHeroComponent::IsMovementInputBlocked() const
{
	PruneMovementInputBlockSources();
	return !MovementInputBlockSources.IsEmpty();
}

void UActHeroComponent::FlushPendingMovementInput(const bool bSuppressNextFrame)
{
	MoveInputVector = FVector2D::ZeroVector;
	bSuppressMovementInputUntilNextSample = bSuppressNextFrame;

	if (APawn* Pawn = GetPawn<APawn>())
	{
		Pawn->ConsumeMovementInputVector();
	}
}

void UActHeroComponent::PruneMovementInputBlockSources() const
{
	for (auto It = MovementInputBlockSources.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
	}
}

void UActHeroComponent::Input_AbilityInputTagPressed(const FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (InputTag.IsValid() && InputTag == InputTag_StrafeMove)
		{
			if (UActBattleComponent* BattleComp = Pawn->FindComponentByClass<UActBattleComponent>())
			{
				// Lock-on acquisition should react on the Shift press itself rather than waiting for the
				// locomotion GA to finish activation, otherwise mid-combo lock attempts feel delayed or lost.
				BattleComp->StartLockOnTarget();
			}
		}

		if (const UActPawnExtensionComponent* PawnExtComp = UActPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (UActAbilitySystemComponent* ActASC = PawnExtComp->GetActAbilitySystemComponent())
			{
				if (AActPlayerController* ActPC = GetController<AActPlayerController>())
				{
					// Route input through controller so command matches can take priority.
					ActPC->QueueAbilityInputTagPressed(InputTag);
				}
				else
				{
					// No controller/analyzer: fall back to direct ASC input.
					ActASC->AbilityInputTagPressed(InputTag);
				}
			}
		}
	}
}

void UActHeroComponent::Input_AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (const APawn* Pawn = GetPawn<APawn>())
	{
		if (InputTag.IsValid() && InputTag == InputTag_StrafeMove)
		{
			if (UActBattleComponent* BattleComp = Pawn->FindComponentByClass<UActBattleComponent>())
			{
				BattleComp->StopLockOnTarget();
			}
		}

		if (const UActPawnExtensionComponent* PawnExtComp = UActPawnExtensionComponent::FindPawnExtensionComponent(Pawn))
		{
			if (UActAbilitySystemComponent* ActASC = PawnExtComp->GetActAbilitySystemComponent())
			{
				if (AActPlayerController* ActPC = GetController<AActPlayerController>())
				{
					ActPC->QueueAbilityInputTagReleased(InputTag);
				}
				else
				{
					ActASC->AbilityInputTagReleased(InputTag);
				}
			}
		}
	}
}

void UActHeroComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	// Cache raw move intent before any gameplay/movement gating so other systems can query the
	// latest authored direction even while locomotion is temporarily blocked.
	MoveInputVector = InputActionValue.Get<FVector2D>();

	APawn* Pawn = GetPawn<APawn>();
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Pawn || !Controller)
	{
		return;
	}

	const FVector2D Value = MoveInputVector;
	const bool bSuppressMovementInputThisSample = bSuppressMovementInputUntilNextSample;
	bSuppressMovementInputUntilNextSample = false;

	const FRotator ControlRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector WorldMoveVector = ControlRot.RotateVector(FVector(Value.Y, Value.X, 0.f));
	const bool bMovementInputBlocked = IsMovementInputBlocked();
	const bool bHasMeaningfulMoveInput = !Value.IsNearlyZero();
	const bool bShouldApplyMovementInput =
		!bMovementInputBlocked &&
		!bSuppressMovementInputThisSample;

	if (bShouldApplyMovementInput && Value.X != 0.f)
	{
		const FVector MovementDirection = ControlRot.RotateVector(FVector::RightVector);
		Pawn->AddMovementInput(MovementDirection, Value.X);
	}

	if (bShouldApplyMovementInput && Value.Y != 0.f)
	{
		const FVector MovementDirection = ControlRot.RotateVector(FVector::ForwardVector);
		Pawn->AddMovementInput(MovementDirection, Value.Y);
	}

	const FVector2D LocalDirection(
		FVector::DotProduct(WorldMoveVector.GetSafeNormal2D(), Pawn->GetActorRightVector().GetSafeNormal2D()),
		FVector::DotProduct(WorldMoveVector.GetSafeNormal2D(), Pawn->GetActorForwardVector().GetSafeNormal2D()));

	if (AActPlayerController* ActPC = GetController<AActPlayerController>())
	{
		// Analyzer consumes character-relative direction rather than world-space direction.
		ActPC->PushDirectionToAnalyzer(QuantizeLocalDirection(LocalDirection));
	}
}

FVector UActHeroComponent::GetMoveInputDirectionFromActor() const
{
	const APawn* Pawn = GetPawn<APawn>();
	if (!Pawn || MoveInputVector.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	FVector MoveDirection =
		Pawn->GetActorRightVector() * MoveInputVector.X +
		Pawn->GetActorForwardVector() * MoveInputVector.Y;
	MoveDirection.Z = 0.0f;
	return MoveDirection.GetSafeNormal();
}

FVector UActHeroComponent::GetMoveInputDirectionFromController() const
{
	const APawn* Pawn = GetPawn<APawn>();
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Pawn || !Controller || MoveInputVector.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	const FRotator ControlYawRotation(0.0f, Controller->GetControlRotation().Yaw, 0.0f);
	FVector MoveDirection = ControlYawRotation.RotateVector(FVector(MoveInputVector.Y, MoveInputVector.X, 0.0f));
	MoveDirection.Z = 0.0f;
	return MoveDirection.GetSafeNormal();
}

void UActHeroComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();

	if (!Pawn)
	{
		return;
	}
	
	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

