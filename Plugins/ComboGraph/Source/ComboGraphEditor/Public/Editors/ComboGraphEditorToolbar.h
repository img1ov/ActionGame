#pragma once

#include "Templates/SharedPointer.h"

class FComboGraphAssetEditor;
class FComboGraphBlueprintEditor;
class FExtender;
class FToolBarBuilder;

class FComboGraphEditorToolbar : public TSharedFromThis<FComboGraphEditorToolbar>
{
public:
	FComboGraphEditorToolbar(const TSharedPtr<FComboGraphAssetEditor>& InAssetEditor) : AssetEditor(InAssetEditor) {}

	void AddComboGraphEditorToolbar(const TSharedPtr<FExtender>& Extender);
	void AddDebuggerToolbar(const TSharedPtr<FExtender>& Extender);

protected:
	/** Pointer back to the asset editor tool that owns us */
	TWeakPtr<FComboGraphAssetEditor> AssetEditor;

	void FillModesToolbar(FToolBarBuilder& ToolbarBuilder);
	void FillDebuggerToolbar(FToolBarBuilder& ToolbarBuilder, TWeakPtr<FComboGraphAssetEditor> InAssetEditor);
};
