// Copyright

#pragma once

#include "CoreMinimal.h"
#include "Components/ComboGraphSystemComponent.h"

#include "ActComboGraphComponent.generated.h"

class UActComboGraphConfig;
class UGameplayTask_StartComboGraph;

UCLASS(ClassGroup = (Act), Blueprintable, Meta = (BlueprintSpawnableComponent))
class ACTGAME_API UActComboGraphComponent : public UComboGraphSystemComponent
{
	GENERATED_BODY()

public:
	UActComboGraphComponent(const FObjectInitializer& ObjectInitializer);

	static UActComboGraphComponent* FindComboGraphComponent(const AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Act|Combat|ComboGraph")
	void SetComboGraphConfig(UActComboGraphConfig* InConfig);

	UFUNCTION(BlueprintPure, Category = "Act|Combat|ComboGraph")
	bool IsComboGraphActive() const;

	bool TryHandleCommand(const FGameplayTag& CommandTag);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	const struct FActComboGraphCommandBinding* FindBinding(const FGameplayTag& CommandTag) const;
	bool StartComboGraphForBinding(const struct FActComboGraphCommandBinding& Binding);
	bool FeedActiveComboGraph(const struct FActComboGraphCommandBinding& Binding) const;
	void ClearActiveTask();

	UFUNCTION()
	void HandleGraphEnded(FGameplayTag EventTag, FGameplayEventData EventData);

private:
	UPROPERTY(EditAnywhere, Category = "Act|Combat|ComboGraph")
	TObjectPtr<UActComboGraphConfig> ComboGraphConfig = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UGameplayTask_StartComboGraph> ActiveComboGraphTask = nullptr;
};
