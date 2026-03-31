// Copyright

#include "Character/ActComboGraphComponent.h"

#include "Abilities/Tasks/GameplayTask_StartComboGraph.h"
#include "Character/ActComboGraphConfig.h"
#include "Utils/ComboGraphBlueprintLibrary.h"

UActComboGraphComponent::UActComboGraphComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UActComboGraphComponent* UActComboGraphComponent::FindComboGraphComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UActComboGraphComponent>() : nullptr;
}

void UActComboGraphComponent::SetComboGraphConfig(UActComboGraphConfig* InConfig)
{
	ComboGraphConfig = InConfig;
}

bool UActComboGraphComponent::IsComboGraphActive() const
{
	return ActiveComboGraphTask != nullptr;
}

bool UActComboGraphComponent::TryHandleCommand(const FGameplayTag& CommandTag)
{
	const FActComboGraphCommandBinding* Binding = FindBinding(CommandTag);
	if (!Binding)
	{
		return false;
	}

	if (IsComboGraphActive())
	{
		return FeedActiveComboGraph(*Binding);
	}

	return StartComboGraphForBinding(*Binding);
}

void UActComboGraphComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearActiveTask();
	Super::EndPlay(EndPlayReason);
}

const FActComboGraphCommandBinding* UActComboGraphComponent::FindBinding(const FGameplayTag& CommandTag) const
{
	if (!ComboGraphConfig || !CommandTag.IsValid())
	{
		return nullptr;
	}

	return ComboGraphConfig->CommandBindings.FindByPredicate(
		[&CommandTag](const FActComboGraphCommandBinding& Binding)
		{
			return Binding.CommandTag == CommandTag && Binding.IsValid();
		});
}

bool UActComboGraphComponent::StartComboGraphForBinding(const FActComboGraphCommandBinding& Binding)
{
	if (!Binding.ComboGraph)
	{
		return false;
	}

	UGameplayTask_StartComboGraph* NewTask = UGameplayTask_StartComboGraph::TaskStartComboGraph(this, Binding.ComboGraph, Binding.InputAction, true);
	if (!NewTask)
	{
		return false;
	}

	ClearActiveTask();

	ActiveComboGraphTask = NewTask;
	ActiveComboGraphTask->OnGraphEnd.AddDynamic(this, &ThisClass::HandleGraphEnded);
	ActiveComboGraphTask->ReadyForActivation();
	return true;
}

bool UActComboGraphComponent::FeedActiveComboGraph(const FActComboGraphCommandBinding& Binding) const
{
	if (!ActiveComboGraphTask || !Binding.InputAction || !GetOwner())
	{
		return false;
	}

	UComboGraphBlueprintLibrary::SimulateComboInput(GetOwner(), Binding.InputAction);
	return true;
}

void UActComboGraphComponent::ClearActiveTask()
{
	if (!ActiveComboGraphTask)
	{
		return;
	}

	ActiveComboGraphTask->OnGraphEnd.RemoveAll(this);
	ActiveComboGraphTask = nullptr;
}

void UActComboGraphComponent::HandleGraphEnded(FGameplayTag EventTag, FGameplayEventData EventData)
{
	ClearActiveTask();
}
