// Fill out your copyright notice in the Description page of Project Settings.


#include "GameModes/ActGameState.h"

#include "AbilitySystem/ActAbilitySystemComponent.h"
#include "Component/BulletSystemComponent.h"
#include "GameModes/ActExperienceManagerComponent.h"
#include "Net/UnrealNetwork.h"

extern ENGINE_API float GAverageFPS;

AActGameState::AActGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<UActAbilitySystemComponent>(this, TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	BulletSystemComponent = ObjectInitializer.CreateDefaultSubobject<UBulletSystemComponent>(this, TEXT("BulletSystemComponent"));
	BulletSystemComponent->SetIsReplicated(true);

	ExperienceManagerComponent = CreateDefaultSubobject<UActExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));

	ServerFPS = 0.f;
}

void AActGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetLocalRole() == ROLE_Authority)
	{
		ServerFPS = GAverageFPS;
	}
}

void AActGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(ThisClass, ServerFPS);
}

float AActGameState::GetServerFPS() const
{
	return ServerFPS;
}

UAbilitySystemComponent* AActGameState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UBulletSystemComponent* AActGameState::GetBulletSystemComponent_Implementation() const
{
	return BulletSystemComponent;
}
