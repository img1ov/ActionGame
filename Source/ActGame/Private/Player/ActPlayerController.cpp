// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ActPlayerController.h"

#include "AbilitySystemGlobals.h"
#include "ActGameplayTags.h"
#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "DisplayDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/HUD.h"
#include "Player/ActPlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ActPlayerController)

namespace
{
FString GetBattlePCContextString(const AActPlayerController* PC)
{
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return FString::Printf(TEXT("PC=%s Pawn=%s Local=%d Auth=%d Time=%.3f"),
		*GetNameSafe(PC),
		*GetNameSafe(Pawn),
		PC && PC->IsLocalController(),
		Pawn && Pawn->HasAuthority(),
		(PC && PC->GetWorld()) ? PC->GetWorld()->GetTimeSeconds() : 0.0);
}

constexpr double AbilityInputRepeatLatchSeconds = 0.075;
}

AActPlayerController::AActPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BattleInputAnalyzer = MakeUnique<FActBattleInputAnalyzer>();
	CommandResolver = MakeUnique<FActBattleCommandResolver>();
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
		if (!PendingAbilityInputPressed.IsEmpty() || !PendingAbilityInputReleased.IsEmpty())
		{
			UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] PostProcessInput begin. %s Pressed=%s Released=%s"),
				*GetBattlePCContextString(this),
				*FGameplayTagContainer::CreateFromArray(PendingAbilityInputPressed).ToString(),
				*FGameplayTagContainer::CreateFromArray(PendingAbilityInputReleased).ToString());
		}

		ResolveBufferedCommand(*ActASC);

		for (const FGameplayTag& InputTag : PendingAbilityInputPressed)
		{
			ActASC->AbilityInputTagPressed(InputTag);
		}

		for (const FGameplayTag& InputTag : PendingAbilityInputReleased)
		{
			ActASC->AbilityInputTagReleased(InputTag);
		}

		PendingAbilityInputPressed.Reset();
		PendingAbilityInputReleased.Reset();

		ActASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	else
	{
		// Defensive: avoid carrying stale input across frames when ASC is unavailable.
		PendingAbilityInputPressed.Reset();
		PendingAbilityInputReleased.Reset();
	}

	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void AActPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (BattleInputAnalyzer.IsValid())
	{
		const double CurrentTime = GetAnalyzerCurrentTimeSeconds();
		BattleInputAnalyzer->Tick(CurrentTime);
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

	const double CurrentTimeSeconds = GetAnalyzerCurrentTimeSeconds();
	const bool bStrictLatch = InputTag.MatchesTag(ActGameplayTags::FindTagByString(TEXT("InputTag.Attack"), true));
	if (double* LastSeenTimeSeconds = ActiveAbilityInputTagLastSeenTimes.Find(InputTag))
	{
		const double ElapsedSeconds = CurrentTimeSeconds - *LastSeenTimeSeconds;
		// Ability inputs should behave as press edges even if the input action is bound
		// with Triggered or otherwise fires repeatedly while the button is held.
		// Use a short latch instead of waiting strictly for release so repeated taps
		// still work even when the action does not emit a clean Completed event.
		if (ElapsedSeconds <= AbilityInputRepeatLatchSeconds)
		{
			// Attacks should not re-arm while the input is held to prevent 1A->1A spam.
			if (bStrictLatch)
			{
				*LastSeenTimeSeconds = CurrentTimeSeconds;
			}
			return;
		}
	}

	ActiveAbilityInputTagLastSeenTimes.FindOrAdd(InputTag) = CurrentTimeSeconds;
	PendingAbilityInputPressed.AddUnique(InputTag);
	PushInputTagPressedToAnalyzer(InputTag);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] QueuePressed. %s Tag=%s Direction=%d"),
		*GetBattlePCContextString(this),
		*InputTag.ToString(),
		static_cast<int32>(CurrentAnalyzerDirection));
}

void AActPlayerController::QueueAbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid())
	{
		return;
	}

	ActiveAbilityInputTagLastSeenTimes.Remove(InputTag);
	PendingAbilityInputReleased.AddUnique(InputTag);

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] QueueReleased. %s Tag=%s"),
		*GetBattlePCContextString(this),
		*InputTag.ToString());
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

	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleInput] Direction. %s Direction=%d"),
		*GetBattlePCContextString(this),
		static_cast<int32>(InputDirection));
}

void AActPlayerController::ConfigureInputCommandDefinitions(const TArray<FInputCommandDefinition>& InCommandDefinitions)
{
	if (!BattleInputAnalyzer.IsValid())
	{
		return;
	}

	BattleInputAnalyzer->SetCommandDefinitions(InCommandDefinitions);
	BattleInputAnalyzer->ResetCommandBuffer();
	if (CommandResolver.IsValid())
	{
		CommandResolver->Reset();
	}
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->ResetAbilityChainRuntime();
	}
	PendingAbilityInputPressed.Reset();
	PendingAbilityInputReleased.Reset();
	ActiveAbilityInputTagLastSeenTimes.Reset();
	CurrentAnalyzerDirection = EInputDirection::Neutral;
}

void AActPlayerController::HandleInputCommandMatched(const FGameplayTag& CommandTag) const
{
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Matched. %s Command=%s"),
		*GetBattlePCContextString(this),
		*CommandTag.ToString());

	InputCommandMatched.Broadcast(CommandTag);
}

void AActPlayerController::ResolveBufferedCommand(UActAbilitySystemComponent& ActASC)
{
	if (!BattleInputAnalyzer.IsValid() || !CommandResolver.IsValid())
	{
		return;
	}

	const double CurrentTime = GetAnalyzerCurrentTimeSeconds();
	BattleInputAnalyzer->Tick(CurrentTime);

	FGameplayTag BufferedCommandTag;
	if (!BattleInputAnalyzer->PeekMatchedCommand(CurrentTime, BufferedCommandTag))
	{
		return;
	}

	bool bShouldConsumeCommand = true;
	const EActBattleCommandResolveResult ResolveResult = CommandResolver->ResolveCommand(ActASC, BufferedCommandTag, bShouldConsumeCommand);
	if (ResolveResult == EActBattleCommandResolveResult::NotHandled)
	{
		return;
	}

	if (bShouldConsumeCommand)
	{
		BattleInputAnalyzer->ConsumeMatchedCommand(CurrentTime, BufferedCommandTag);
		ResetCommandInputBuffer();
	}
	UE_LOG(LogActAbilitySystem, Verbose, TEXT("[BattleCommand] Resolve activated. %s Command=%s"),
		*GetBattlePCContextString(this),
		*BufferedCommandTag.ToString());
	DebugPrintCommandExecution(BufferedCommandTag);
}

void AActPlayerController::DebugPrintCommandExecution(const FGameplayTag& CommandTag) const
{
#if WITH_EDITOR
	if (!BattleInputAnalyzer.IsValid() || !CommandTag.IsValid())
	{
		return;
	}
	if (!GEngine)
	{
		return;
	}

	const AHUD* HUD = GetHUD();
	if (!HUD || !HUD->bShowDebugInfo)
	{
		return;
	}

	const FDebugDisplayInfo DebugDisplay(HUD->DebugDisplay, HUD->ToggledDebugCategories);
	if (!DebugDisplay.IsDisplayOn(TEXT("BattleCommandResolver")))
	{
		return;
	}

	TArray<FString> DebugLines;
	BattleInputAnalyzer->BuildDebugLines(DebugLines, CommandTag);
	if (DebugLines.IsEmpty())
	{
		return;
	}

	for (int32 Index = 0; Index < DebugLines.Num(); ++Index)
	{
		const FColor Color = (Index == 0) ? FColor::Yellow : FColor::Cyan;
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, DebugLines[Index]);
	}
#endif
}

void AActPlayerController::ResetCommandInputBuffer()
{
	if (!BattleInputAnalyzer.IsValid())
	{
		return;
	}

	BattleInputAnalyzer->ResetCommandBuffer();
	CurrentAnalyzerDirection = EInputDirection::Neutral;
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

void AActPlayerController::ClearAbilityChainCache()
{
	if (UActAbilitySystemComponent* ActASC = GetActAbilitySystemComponent())
	{
		ActASC->ResetAbilityChainRuntime();
	}
}

void AActPlayerController::OnPlayerStateChanged()
{
	// Empty, place for derived classes to implement without having to hook all the other events
}
