// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ActPlayerController.h"

#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Player/ActPlayerState.h"

AActPlayerController::AActPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BattleInputAnalyzer = MakeUnique<FActBattleInputAnalyzer>();
	ComboRuntime = MakeUnique<FActCommandRuntimeResolver>();
	BattleInputAnalyzer->SetOnCommandMatchedDelegate(
		FActOnInputCommandMatched::CreateUObject(this, &ThisClass::HandleInputCommandMatched));
}

AActPlayerController::~AActPlayerController() = default;

AActPlayerState* AActPlayerController::GetActPlayerState() const
{
	return CastChecked<AActPlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

UActAbilitySystemComponent* AActPlayerController::GetActAbilitySystemComponent() const
{
	const AActPlayerState* ActPS = GetActPlayerState();
	return (ActPS ? ActPS->GetActAbilitySystemComponent() : nullptr);
}

UAbilitySystemComponent* AActPlayerController::GetAbilitySystemComponent() const
{
	return GetActAbilitySystemComponent();
}

void AActPlayerController::PreProcessInput(const float DeltaTime, const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void AActPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		if (!bCommandMatchedThisFrame)
		{
			for (const FGameplayTag& InputTag : PendingAbilityInputPressed)
			{
				ActASC->AbilityInputTagPressed(InputTag);
			}
		}

		for (const FGameplayTag& InputTag : PendingAbilityInputReleased)
		{
			ActASC->AbilityInputTagReleased(InputTag);
		}

		PendingAbilityInputPressed.Reset();
		PendingAbilityInputReleased.Reset();
		bCommandMatchedThisFrame = false;

		ActASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	else
	{
		// Defensive: avoid carrying stale input across frames when ASC is unavailable.
		PendingAbilityInputPressed.Reset();
		PendingAbilityInputReleased.Reset();
		bCommandMatchedThisFrame = false;
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void AActPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (BattleInputAnalyzer.IsValid() && ComboRuntime.IsValid())
	{
		const double CurrentTime = GetAnalyzerCurrentTimeSeconds();
		BattleInputAnalyzer->Tick(CurrentTime);

		if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
		{
			FGameplayTag BufferedCommandTag;
			if (BattleInputAnalyzer->PeekMatchedCommand(CurrentTime, BufferedCommandTag))
			{
				FGameplayTagContainer StateTags;
				BuildAnalyzerStateTags(StateTags);
				if (ComboRuntime->TryExecuteCommand(*ActASC, BufferedCommandTag, StateTags))
				{
					BattleInputAnalyzer->ConsumeMatchedCommand(CurrentTime, BufferedCommandTag);
				}

				// Keep buffered command for next tick.
			}
		}
	}
}

void AActPlayerController::PushInputTagPressedToAnalyzer(const FGameplayTag& InputTag) const
{
	if (!BattleInputAnalyzer.IsValid() || !InputTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer StateTags;
	BuildAnalyzerStateTags(StateTags);
	BattleInputAnalyzer->AddInputTagPressed(InputTag, GetAnalyzerCurrentTimeSeconds(), CurrentAnalyzerDirection, StateTags);
}


void AActPlayerController::QueueAbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	PendingAbilityInputPressed.AddUnique(InputTag);
	PushInputTagPressedToAnalyzer(InputTag);
}

void AActPlayerController::QueueAbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	PendingAbilityInputReleased.AddUnique(InputTag);
}

void AActPlayerController::PushDirectionToAnalyzer(const EInputDirection InputDirection)
{
	if (!BattleInputAnalyzer.IsValid())
	{
		return;
	}

	CurrentAnalyzerDirection = InputDirection;

	FGameplayTagContainer StateTags;
	BuildAnalyzerStateTags(StateTags);
	BattleInputAnalyzer->AddDirectionalInput(InputDirection, GetAnalyzerCurrentTimeSeconds(), StateTags);
}

void AActPlayerController::ConfigureInputCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions)
{
	if (!BattleInputAnalyzer.IsValid())
	{
		return;
	}

	BattleInputAnalyzer->SetCommandDefinitions(InCommandDefinitions);
	BattleInputAnalyzer->ResetInputHistory();
	BattleInputAnalyzer->ResetMatchedCommands();
	if (ComboRuntime.IsValid())
	{
		ComboRuntime->Reset();
	}
	PendingAbilityInputPressed.Reset();
	PendingAbilityInputReleased.Reset();
	bCommandMatchedThisFrame = false;
	CurrentAnalyzerDirection = EInputDirection::Neutral;
}

void AActPlayerController::HandleInputCommandMatched(const FGameplayTag& CommandTag) const
{
	AActPlayerController* MutableThis = const_cast<AActPlayerController*>(this);
	MutableThis->bCommandMatchedThisFrame = true;
	InputCommandMatched.Broadcast(CommandTag);
}

void AActPlayerController::BuildAnalyzerStateTags(FGameplayTagContainer& OutStateTags) const
{
	OutStateTags.Reset();

	if (const UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->GetOwnedGameplayTags(OutStateTags);
	}
}

double AActPlayerController::GetAnalyzerCurrentTimeSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void AActPlayerController::RegisterAbilityChainWindow(
	const FName WindowId,
	const TArray<FActAbilityChainEntry>& ChainEntries)
{
	if (!ComboRuntime.IsValid())
	{
		return;
	}

	ComboRuntime->RegisterAbilityChainWindow(WindowId, ChainEntries);
}

void AActPlayerController::UnregisterAbilityChainWindow(const FName WindowId)
{
	if (!ComboRuntime.IsValid())
	{
		return;
	}

	ComboRuntime->UnregisterAbilityChainWindow(WindowId);
}

void AActPlayerController::ClearAbilityChainCache()
{
	if (ComboRuntime.IsValid())
	{
		ComboRuntime->Reset();
	}
}
