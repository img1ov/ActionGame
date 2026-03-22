// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ActPlayerController.h"

#include "AbilitySystemGlobals.h"
#include "ActLogChannels.h"
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

void AActPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AActPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetActorHiddenInGame(false);
}

void AActPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void AActPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AActPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
}

void AActPlayerController::OnUnPossess()
{
	// Make sure the pawn that is being unpossessed doesn't remain our ASC's avatar actor
	if (const APawn* PawnBeingUnpossessed = GetPawn())
	{
		const APlayerState* ThePlayerState = PlayerState.Get();
		if (IsValid(ThePlayerState))
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(ThePlayerState))
			{
				if (ASC->GetAvatarActor() == PawnBeingUnpossessed)
				{
					ASC->SetAvatarActor(nullptr);
				}
			}
		}
	}

	Super::OnUnPossess();
}

void AActPlayerController::InitPlayerState()
{
	Super::InitPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AActPlayerController::CleanupPlayerState()
{
	Super::CleanupPlayerState();
	BroadcastOnPlayerStateChanged();
}

void AActPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BroadcastOnPlayerStateChanged();

	// When we're a client connected to a remote server, the player controller may replicate later than the PlayerState and AbilitySystemComponent.
	// However, TryActivateAbilitiesOnSpawn depends on the player controller being replicated in order to check whether on-spawn abilities should
	// execute locally. Therefore once the PlayerController exists and has resolved the PlayerState, try once again to activate on-spawn abilities.
	// On other net modes the PlayerController will never replicate late, so ActASC's own TryActivateAbilitiesOnSpawn calls will succeed. The handling 
	// here is only for when the PlayerState and ASC replicated before the PC and incorrectly thought the abilities were not for the local player.
	if (GetWorld()->IsNetMode(NM_Client))
	{
		if (AActPlayerState* ActPS = GetPlayerState<AActPlayerState>())
		{
			if (UActAbilitySystemComponent* ActASC = ActPS->GetActAbilitySystemComponent())
			{
				ActASC->RefreshAbilityActorInfo();
				ActASC->TryActivateAbilitiesOnSpawn();
			}
		}
	}
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

void AActPlayerController::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	UE_LOG(LogActTeams, Error, TEXT("You can't set the team ID on a player controller (%s); it's driven by the associated player state"), *GetPathNameSafe(this));
}

FGenericTeamId AActPlayerController::GetGenericTeamId() const
{
	if (const IActTeamAgentInterface* PSWithTeamInterface = Cast<IActTeamAgentInterface>(PlayerState))
	{
		return PSWithTeamInterface->GetGenericTeamId();
	}
	
	return FGenericTeamId::NoTeam;
}

FOnActTeamIndexChangedDelegate* AActPlayerController::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
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

void AActPlayerController::BroadcastOnPlayerStateChanged()
{
	OnPlayerStateChanged();
	
	// Unbind from the old player state, if any
	FGenericTeamId OldTeamID = FGenericTeamId::NoTeam;
	if (LastSeenPlayerState != nullptr)
	{
		if (IActTeamAgentInterface* PlayerStateTeamInterface = Cast<IActTeamAgentInterface>(LastSeenPlayerState))
		{
			OldTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().RemoveAll(this);
		}
	}
	
	// Bind to the new player state, if any
	FGenericTeamId NewTeamID = FGenericTeamId::NoTeam;
	if (PlayerState != nullptr)
	{
		if (IActTeamAgentInterface* PlayerStateTeamInterface = Cast<IActTeamAgentInterface>(PlayerState))
		{
			NewTeamID = PlayerStateTeamInterface->GetGenericTeamId();
			PlayerStateTeamInterface->GetTeamChangedDelegateChecked().AddDynamic(this, &ThisClass::OnPlayerStateChangedTeam);
		}
	}
	
	// Broadcast the team change (if it really has)
	ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);
	
	LastSeenPlayerState = PlayerState;
}

void AActPlayerController::OnPlayerStateChangedTeam(UObject* TeamAgent, int32 OldTeam, int32 NewTeam)
{
	ConditionalBroadcastTeamChanged(this, IntegerToGenericTeamId(OldTeam), IntegerToGenericTeamId(NewTeam));
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

void AActPlayerController::OnPlayerStateChanged()
{
	// Empty, place for derived classes to implement without having to hook all the other events
}
