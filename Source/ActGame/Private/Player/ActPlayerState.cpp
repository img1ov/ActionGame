// Fill out your copyright notice in the Description page of Project Settings.


#include "Player/ActPlayerState.h"

#include "ActLogChannels.h"
#include "AbilitySystem/ActAbilitySet.h"
#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/ActCombatSet.h"
#include "AbilitySystem/Attributes/ActHealthSet.h"
#include "Component/BulletSystemComponent.h"
#include "Player/ActPlayerController.h"
#include "Character/ActPawnData.h"
#include "GameModes/ActExperienceManagerComponent.h"
#include "GameModes/ActGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"

class AController;
class APlayerState;
class FLifetimeProperty;

AActPlayerState::AActPlayerState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UActAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	BulletSystemComponent = ObjectInitializer.CreateDefaultSubobject<UBulletSystemComponent>(this, TEXT("BulletSystemComponent"));
	BulletSystemComponent->SetIsReplicated(true);

	// These attribute sets will be detected by AbilitySystemComponent::InitializeComponent. Keeping a reference so that the sets don't get garbage collected before that.
	HealthSet = CreateDefaultSubobject<UActHealthSet>(TEXT("HealthSet"));
	CombatSet = CreateDefaultSubobject<UActCombatSet>(TEXT("CombatSet"));

	// AbilitySystemComponent needs to be updated at a high frequency.
	SetNetUpdateFrequency(100.0f);
	
	MyTeamID = FGenericTeamId::NoTeam;
}

AActPlayerController* AActPlayerState::GetActPlayerController() const
{
	return Cast<AActPlayerController>(GetOwningController());
}

UAbilitySystemComponent* AActPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UBulletSystemComponent* AActPlayerState::GetBulletSystemComponent_Implementation() const
{
	return BulletSystemComponent;
}

void AActPlayerState::SetPawnData(const UActPawnData* InPawnData)
{
	check(InPawnData);

	if (GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogAct, Error, TEXT("Trying to set PawnData [%s] on player state [%s] that already has valid PawnData [%s]."), *GetNameSafe(InPawnData), *GetNameSafe(this), *GetNameSafe(PawnData));
		return;
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PawnData, this);
	PawnData = InPawnData;

	for (const UActAbilitySet* AbilitySet : PawnData->AbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, nullptr);
		}
	}
	
	ForceNetUpdate();
}

void AActPlayerState::PreInitializeComponents()
{
	Super::PreInitializeComponents();
}

void AActPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	check(AbilitySystemComponent);
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && World->GetNetMode() != NM_Client)
	{
		AGameStateBase* GameState = GetWorld()->GetGameState();
		check(GameState);
		UActExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UActExperienceManagerComponent>();
		check(ExperienceComponent);

		ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnActExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
	}
}

void AActPlayerState::SetGenericTeamId(const FGenericTeamId& NewTeamID)
{
	if (HasAuthority())
	{
		const FGenericTeamId OldTeamID = MyTeamID;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, MyTeamID, this);
		MyTeamID = NewTeamID;
		ConditionalBroadcastTeamChanged(this, OldTeamID, NewTeamID);
		ForceNetUpdate();
	}
	else
	{
		UE_LOG(LogActTeams, Error, TEXT("Cannot set team for %s on non-authority"), *GetPathName(this));
	}
}

FGenericTeamId AActPlayerState::GetGenericTeamId() const
{
	return MyTeamID;
}

FOnActTeamIndexChangedDelegate* AActPlayerState::GetOnTeamIndexChangedDelegate()
{
	return &OnTeamChangedDelegate;
}

void AActPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, PawnData, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, MyTeamID, SharedParams);
}

void AActPlayerState::OnRep_PawnData()
{
}

void AActPlayerState::OnExperienceLoaded(const UActExperienceDefinition* CurrentExperience)
{
	if (AActGameMode* ActGameMode = GetWorld()->GetAuthGameMode<AActGameMode>())
	{
		if (const UActPawnData* NewPawnData = ActGameMode->GetPawnDataForController(GetOwningController()))
		{
			SetPawnData(NewPawnData);
		}
		else
		{
			UE_LOG(LogAct, Error, TEXT("AActPlayerState::OnExperienceLoaded(): Unable to find PawnData to initialize player state [%s]!"), *GetNameSafe(this));
		}
	}
}

void AActPlayerState::OnRep_MyTeamID(FGenericTeamId OldTeamID)
{
	ConditionalBroadcastTeamChanged(this, OldTeamID, MyTeamID);
}
