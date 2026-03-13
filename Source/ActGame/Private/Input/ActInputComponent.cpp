// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/ActInputComponent.h"

UActInputComponent::UActInputComponent(const FObjectInitializer& ObjectInitializer)
{
}

void UActInputComponent::AddInputMappings(const UActInputConfig* InputConfig,
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// Extension point for mode-specific mapping layers (mount/vehicle/UI).
}

void UActInputComponent::RemoveInputMappings(const UActInputConfig* InputConfig,
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem) const
{
	check(InputConfig);
	check(InputSubsystem);

	// Paired cleanup point for AddInputMappings.
}

void UActInputComponent::RemoveBinds(TArray<uint32>& BindHandles)
{
	// Binding ownership is handle-based; centralized removal is the safest approach.
	for (uint32 Handle : BindHandles)
	{
		RemoveBindingByHandle(Handle);
	}
	BindHandles.Reset();
}
