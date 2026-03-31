#pragma once

#include "CoreMinimal.h"
#include "EdGraphUtilities.h"

struct FComboGraphNodeFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override;
};
