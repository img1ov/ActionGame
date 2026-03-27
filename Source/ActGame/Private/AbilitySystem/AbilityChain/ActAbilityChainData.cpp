#include "AbilitySystem/AbilityChain/ActAbilityChainData.h"

const FActAbilityChainNode* UActAbilityChainData::FindNode(const FName NodeId) const
{
	if (NodeId.IsNone())
	{
		return nullptr;
	}

	for (const FActAbilityChainNode& Node : Nodes)
	{
		if (Node.NodeId == NodeId)
		{
			return &Node;
		}
	}

	return nullptr;
}

const FActAbilityChainNode* UActAbilityChainData::GetStartNode() const
{
	if (const FActAbilityChainNode* StartNode = FindNode(StartNodeId))
	{
		return StartNode;
	}

	return Nodes.IsEmpty() ? nullptr : &Nodes[0];
}

FName UActAbilityChainData::GetStartSectionName() const
{
	const FActAbilityChainNode* StartNode = GetStartNode();
	return StartNode ? StartNode->SectionName : NAME_None;
}
