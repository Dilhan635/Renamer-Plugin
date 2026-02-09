// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUI_WorkEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	TSharedRef<class SDockTab> OnSpawnToolkitTab(const class FSpawnTabArgs& Args);

	void RenameSelectedAssets();
	void UpdatePreviewList();
	void ScanForMissingPrefixes();
	bool HasCorrectPrefix(const FAssetData& Asset);

	TSharedPtr<class SEditableTextBox> RenameTextBox;

	struct FPreviewItem
	{
		FString DisplayName;
		FAssetData Asset;
	};

	TSharedPtr<class SListView<TSharedPtr<FPreviewItem>>> PreviewListView;
	TArray<TSharedPtr<FPreviewItem>> PreviewItems;

	FDelegateHandle SelectionChangedHandle;
};
