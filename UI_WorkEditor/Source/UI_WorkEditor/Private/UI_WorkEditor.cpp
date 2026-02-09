// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI_WorkEditor.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateIconFinder.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FUI_WorkEditorModule"

void FUI_WorkEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		"UI_WorkToolKit",
		FOnSpawnTab::CreateRaw(this, &FUI_WorkEditorModule::OnSpawnToolkitTab)
	)
		.SetDisplayName(LOCTEXT("UI_WorkToolkitTab", "Renamer"))
		.SetMenuType(ETabSpawnerMenuType::Enabled);

	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	SelectionChangedHandle = CB.GetOnAssetSelectionChanged().AddLambda(
		[this](const TArray<FAssetData>& NewSelectedAssets, bool bIsPrimaryBrowser)
		{
			UpdatePreviewList();
		}
	);
}

void FUI_WorkEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		CB.GetOnAssetSelectionChanged().Remove(SelectionChangedHandle);
	}

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner("UI_WorkToolKit");

}

static const FSlateBrush* GetIconForAsset(const FAssetData& AssetData)
{
	if (UClass* Class = AssetData.GetClass())
	{
		return FSlateIconFinder::FindIconBrushForClass(Class);
	}

	return FAppStyle::GetBrush("Icons.Question");
}

static FString GetPrefixForAsset(const FAssetData& AssetData)
{
	UClass* Class = AssetData.GetClass();

	if (AssetData.AssetClassPath.GetAssetName() == TEXT("WidgetBlueprint"))
	{
		return TEXT("WBP_");
	}

	if (!Class)
	{
		return TEXT("");
	}

	if (Class->IsChildOf(UBlueprint::StaticClass()))
	{
		return TEXT("BP_");
	}

	if (Class->IsChildOf(UStaticMesh::StaticClass()))
	{
		return TEXT("SM_");
	}

	if (Class->IsChildOf(UMaterial::StaticClass()))
	{
		return TEXT("M_");
	}

	if (Class->IsChildOf(UTexture2D::StaticClass()))
	{
		return TEXT("T_");
	}

	return TEXT("");
}

bool FUI_WorkEditorModule::HasCorrectPrefix(const FAssetData& Asset)
{
	const FString ExpectedPrefix = GetPrefixForAsset(Asset);
	const FString Name = Asset.AssetName.ToString();

	if (ExpectedPrefix.IsEmpty())
		return true;

	return Name.StartsWith(ExpectedPrefix, ESearchCase::IgnoreCase);
}

void FUI_WorkEditorModule::ScanForMissingPrefixes()
{
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TArray<FAssetData> AssetsToScan;
	CB.Get().GetSelectedAssets(AssetsToScan);

	if (AssetsToScan.Num() == 0)
	{
		FARFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.PackagePaths.Add("/Game");

		Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UMaterial::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(UTexture2D::StaticClass()->GetClassPathName());
		Filter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/UMG"), TEXT("WidgetBlueprint")));

		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		AssetRegistry.GetAssets(Filter, AssetsToScan);
	}


	PreviewItems.Empty();

	for (const FAssetData& Asset : AssetsToScan) 
	{
		if (!HasCorrectPrefix(Asset))
		{
			TSharedPtr<FPreviewItem> Item = MakeShared<FPreviewItem>();
			Item->Asset = Asset;
			Item->DisplayName = Asset.AssetName.ToString();
			PreviewItems.Add(Item);
		}
	}

	if (PreviewListView.IsValid())
	{
		PreviewListView->RequestListRefresh();
	}
}

TSharedRef<SDockTab> FUI_WorkEditorModule::OnSpawnToolkitTab(const FSpawnTabArgs& Args) {

	  TSharedRef<SDockTab> Tab =
		  SNew(SDockTab)
		  .TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(STextBlock)
						.Text(FText::FromString("Rename Selected Assets"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SAssignNew(RenameTextBox, SEditableTextBox)
						.HintText(FText::FromString("Enter new base name"))
						.OnTextChanged_Lambda([this](const FText&)
							{
								UpdatePreviewList();
							})
						.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
							{
								if (CommitType == ETextCommit::OnEnter)
								{
									RenameSelectedAssets();
								}
							})
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(SButton)
						.Text(FText::FromString("Rename Assets"))
						.IsEnabled_Lambda([this]()
							{
								if (!RenameTextBox.IsValid())
									return false;

								if (RenameTextBox->GetText().IsEmpty())
									return false;

								FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
								TArray<FAssetData> SelectedAssets;
								CB.Get().GetSelectedAssets(SelectedAssets);

								return SelectedAssets.Num() > 0;
							})
						.OnClicked_Lambda([this]() 
							{
								RenameSelectedAssets();
								return FReply::Handled();
							})
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(STextBlock)
						.Text(FText::FromString("Preview"))
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(10)
				[
					SAssignNew(PreviewListView, SListView<TSharedPtr<FPreviewItem>>)
						.ListItemsSource(&PreviewItems)
						.OnGenerateRow_Lambda([](TSharedPtr<FPreviewItem> Item, const TSharedRef<STableViewBase>& OwnerTable)
							{
								return SNew(STableRow<TSharedPtr<FPreviewItem>>, OwnerTable)
									[
										SNew(SBorder)
											.Padding(0)
											.OnMouseButtonDown_Lambda([Item](const FGeometry&, const FPointerEvent&)
												{
													FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
													TArray<FAssetData> Assets;
													Assets.Add(Item->Asset);

													CB.Get().SyncBrowserToAssets(Assets);

													return FReply::Handled();
												})
											.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
											.BorderBackgroundColor_Lambda([Item]()
												{
													UClass* Class = Item->Asset.GetClass();

													if (!Class)
														return FLinearColor(0.1f, 0.1f, 0.1f, 0.15f);

													if (Item->Asset.AssetClassPath.GetAssetName() == TEXT("WidgetBlueprint"))
														return FLinearColor(0.10f, 0.70f, 0.70f, 0.25f);

													if (Class->IsChildOf(UBlueprint::StaticClass()))
														return FLinearColor(0.10f, 0.35f, 0.80f, 0.25f);

													if (Class->IsChildOf(UStaticMesh::StaticClass()))
														return FLinearColor(0.75f, 0.20f, 0.20f, 0.25f);

													if (Class->IsChildOf(UMaterial::StaticClass()))
														return FLinearColor(0.90f, 0.80f, 0.20f, 0.25f);

													if (Class->IsChildOf(UTexture2D::StaticClass()))
														return FLinearColor(0.20f, 0.65f, 0.30f, 0.25f);

													return FLinearColor(0.1f, 0.1f, 0.1f, 0.15f);
												})

											[
												SNew(SHorizontalBox)

													+ SHorizontalBox::Slot()
													.AutoWidth()
													.VAlign(VAlign_Center)
													[
														SNew(SImage)
															.Image(GetIconForAsset(Item->Asset))
													]

													+ SHorizontalBox::Slot()
													.Padding(8, 0)
													.VAlign(VAlign_Center)
													[
														SNew(STextBlock)
															.Text(FText::FromString(Item->DisplayName))
													]
											]
									];
									
							})
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(10)
				[
					SNew(SButton)
						.Text(FText::FromString("Scan for missing Prefixes"))
						.OnClicked_Lambda([this]()
					{
						ScanForMissingPrefixes();
						return FReply::Handled();
					})
				]
		];

		UpdatePreviewList();
		return Tab;
}

void FUI_WorkEditorModule::RenameSelectedAssets()
{
	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TArray<FAssetData> SelectedAssets;
	CB.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("No asset selected"));
		return;
	}

	FString BaseName = RenameTextBox->GetText().ToString();
	if (BaseName.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("No base name entered"));
		return;
	}

	FAssetToolsModule& AssetTools = FAssetToolsModule::GetModule();
	TArray<FAssetRenameData> RenameDataArray;

	for (int32 i = 0; i < SelectedAssets.Num(); i++)
	{
		const FAssetData& Asset = SelectedAssets[i];

		FString Prefix = GetPrefixForAsset(Asset);
		FString NewName = Prefix + BaseName ;

		if (SelectedAssets.Num() > 1) 
		{
			NewName += FString::Printf(TEXT("_%02d"), i + 1);
		}

		FString PackagePath = FPackageName::GetLongPackagePath(SelectedAssets[i].ObjectPath.ToString());

		RenameDataArray.Add(FAssetRenameData(
			SelectedAssets[i].GetAsset(),
			PackagePath,
			NewName
		));
	}

	AssetTools.Get().RenameAssets(RenameDataArray);

	UE_LOG(LogTemp, Error, TEXT("Renamed %d assets."), SelectedAssets.Num());

}

void FUI_WorkEditorModule::UpdatePreviewList()
{
	PreviewItems.Empty();

	FContentBrowserModule& CB = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	TArray<FAssetData> SelectedAssets;
	CB.Get().GetSelectedAssets(SelectedAssets);

	if (SelectedAssets.Num() == 0)
	{
		PreviewItems.Add(MakeShared<FPreviewItem>(FPreviewItem{
			TEXT("No asset selected."),
			FAssetData()
			}));
	}
	else
	{
		FString BaseName = RenameTextBox.IsValid()
			? RenameTextBox->GetText().ToString()
			: TEXT("");

		for (int32 i = 0; i < SelectedAssets.Num(); i++)
		{
			const FAssetData& Asset = SelectedAssets[i];

			FString Prefix = GetPrefixForAsset(Asset);
			FString NewName = Prefix + BaseName;

			if (SelectedAssets.Num() > 1)
			{
				NewName += FString::Printf(TEXT("_%02d"), i + 1);
			}

			PreviewItems.Add(MakeShared<FPreviewItem>(FPreviewItem{
				NewName,
				Asset
				}));
		}
	}

	if (PreviewListView.IsValid())
	{
		PreviewListView->RequestListRefresh();
	}
}


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUI_WorkEditorModule, UI_WorkEditor)