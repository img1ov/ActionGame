
#include "Graph/ComboGraphNodeBase.h"

#include "ComboGraphLog.h"
#include "Graph/ComboGraphEdge.h"
#include "Graph/ComboGraphNodeSequence.h"


#define LOCTEXT_NAMESPACE "ComboGraphNode"

FText UComboGraphNodeBase::GetNodeTitle() const
{
	FText DefaultText = LOCTEXT("DefaultNodeTitle", "Combat Graph Node");
	return NodeTitle.IsEmpty() ? DefaultText : NodeTitle;
}

UComboGraphEdge* UComboGraphNodeBase::GetEdge(UComboGraphNodeBase* ChildNode)
{
	if (Edges.Contains(ChildNode))
	{
		return Edges.FindChecked(ChildNode);
	}

	return GetEdgeComparingStartEndNodes(ChildNode);
}

bool UComboGraphNodeBase::IsLeafNode() const
{
	return ChildrenNodes.Num() == 0;
}

UComboGraphEdge* UComboGraphNodeBase::GetEdgeWithInput(UInputAction* InputAction)
{
	check(InputAction);

	UComboGraphEdge* Result = nullptr;

	for (UComboGraphNodeBase* ChildNode : ChildrenNodes)
	{
		UComboGraphEdge* Edge = GetEdge(ChildNode);
		if (Edge && Edge->TransitionInput == InputAction)
		{
			Result = Edge;
			break;
		}
	}

	return Result;
}

bool UComboGraphNodeBase::IsComboSubclassedInBlueprint() const
{
	const UClass* NodeClass = GetClass();
	if (!NodeClass)
	{
		return false;
	}

	return !NodeClass->IsNative();
}

#if WITH_EDITOR
bool UComboGraphNodeBase::IsNameEditable() const
{
	return true;
}

void UComboGraphNodeBase::SetNodeTitle(const FText& NewTitle)
{
	NodeTitle = NewTitle;
}

bool UComboGraphNodeBase::CanCreateConnection(UComboGraphNodeBase* Other, FText& ErrorMessage)
{
	return true;
}

bool UComboGraphNodeBase::CanCreateConnectionTo(UComboGraphNodeBase* Other, const int32 NumberOfChildrenNodes, FText& ErrorMessage)
{
	if (ChildrenLimitType == EACENodeLimit::Limited && NumberOfChildrenNodes >= ChildrenLimit)
	{
		ErrorMessage = FText::FromString("Children limit exceeded");
		return false;
	}

	return CanCreateConnection(Other, ErrorMessage);
}

bool UComboGraphNodeBase::CanCreateConnectionFrom(UComboGraphNodeBase* Other, const int32 NumberOfParentNodes, FText& ErrorMessage)
{
	if (ParentLimitType == EACENodeLimit::Limited && NumberOfParentNodes >= ParentLimit)
	{
		ErrorMessage = FText::FromString("Parent limit exceeded");
		return false;
	}

	return true;
}

#endif

UComboGraphEdge* UComboGraphNodeBase::GetEdgeComparingStartEndNodes(UComboGraphNodeBase* ChildNode)
{
	UComboGraphEdge* FoundEdge = nullptr;
	for (const TPair<TObjectPtr<UComboGraphNodeBase>, TObjectPtr<UComboGraphEdge>>& Pair : Edges)
	{
		UComboGraphEdge* ComboGraphEdge = Pair.Value;
		if (ComboGraphEdge && ComboGraphEdge->StartNode == this && ComboGraphEdge->EndNode == ChildNode)
		{
			FoundEdge = ComboGraphEdge;
			break;
		}
	}

	CG_RUNTIME_LOG(
		Verbose,
		TEXT("GetEdgeComparingStartEndNodes: Fallback to comparing start / end nodes for ChildNode: %s (%p) - FoundEdge: %s"),
		*GetNameSafe(ChildNode),
		ChildNode,
		*GetNameSafe(FoundEdge)
	)
	
	return FoundEdge;
}

#undef LOCTEXT_NAMESPACE
