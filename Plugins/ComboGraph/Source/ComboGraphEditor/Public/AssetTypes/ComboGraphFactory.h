#pragma once

#include "Engine/Blueprint.h"
#include "Factories/Factory.h"
#include "ComboGraphFactory.generated.h"

UCLASS(HideCategories=Object, MinimalAPI)
class UComboGraphFactory : public UFactory
{
	GENERATED_BODY()

public:
	UComboGraphFactory();

	// The type of blueprint that will be created
	UPROPERTY(EditAnywhere, Category=GameplayAbilitiesFactory)
	TEnumAsByte<EBlueprintType> BlueprintType;

	// The parent class of the created blueprint
	// UPROPERTY(EditAnywhere, Category=GameplayAbilitiesFactory)
	// TSubclassOf<class UGameplayAbility> ParentClass;

	//~ Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface
};
