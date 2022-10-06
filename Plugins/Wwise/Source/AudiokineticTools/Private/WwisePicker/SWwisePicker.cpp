/*******************************************************************************
The content of the files in this repository include portions of the
AUDIOKINETIC Wwise Technology released in source code form as part of the SDK
package.

Commercial License Usage

Licensees holding valid commercial licenses to the AUDIOKINETIC Wwise Technology
may use these files in accordance with the end user license agreement provided
with the software or, alternatively, in accordance with the terms contained in a
written agreement between you and Audiokinetic Inc.

Copyright (c) 2021 Audiokinetic Inc.
*******************************************************************************/


/*------------------------------------------------------------------------------------
	SWwisePicker.cpp
------------------------------------------------------------------------------------*/

#include "SWwisePicker.h"
#include "Wwise/WwiseProjectDatabaseDelegates.h"
#include "WwisePicker/WwisePickerViewCommands.h"

#include "AkAudioBankGenerationHelpers.h"
#include "AkAudioDevice.h"
#include "AkAudioStyle.h"
#include "AkAudioType.h"
#include "AkSettings.h"
#include "AkUnrealHelper.h"
#include "IAudiokineticTools.h"
#include "AssetManagement/AkAssetDatabase.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor/UnrealEd/Public/EditorDirectories.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDesktopPlatform.h"
#include "Async/Async.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Wwise/WwiseProjectDatabase.h"
#include "Wwise/Metadata/WwiseMetadataProjectInfo.h"
#include "WwisePicker/WwiseAssetDragDropOp.h"
#include "WwisePicker/WwisePickerHelpers.h"
#include "WwisePicker/WwisePickerDataLoader.h"

#define LOCTEXT_NAMESPACE "AkAudio"

const FName SWwisePicker::WwisePickerTabName = "WwisePicker";

namespace SWwisePicker_Helper
{
	const FName DirectoryWatcherModuleName = "DirectoryWatcher";
}

SWwisePicker::SWwisePicker(): CommandList(MakeShared<FUICommandList>())
{
	AllowTreeViewDelegates = true;
	DataLoader = MakeUnique<FWwisePickerDataLoader>();
	OnDatabaseUpdateCompleteHandle = FWwiseProjectDatabaseDelegates::Get().GetOnDatabaseUpdateCompletedDelegate().AddLambda([this]
	{
		AsyncTask(ENamedThreads::Type::GameThread, [this]
		{
			this->ForceRefresh();
		});
	});
}

void SWwisePicker::CreateWwisePickerCommands()
{
	const FWwisePickerViewCommands& Commands = FWwisePickerViewCommands::Get();
	FUICommandList& ActionList = *CommandList;

	// Action for importing the selected items from the Waapi Picker.
	ActionList.MapAction(
		Commands.RequestImportWwiseItem,
		FExecuteAction::CreateSP(this, &SWwisePicker::HandleImportWwiseItemCommandExecute));
}

TSharedPtr<SWidget> SWwisePicker::MakeWwisePickerContextMenu()
{
	const FWwisePickerViewCommands& Commands = FWwisePickerViewCommands::Get();

	// Build up the menu
	FMenuBuilder MenuBuilder(true, CommandList);
	{
		MenuBuilder.BeginSection("WaapiPickerImport");
		{
			MenuBuilder.AddMenuEntry(Commands.RequestImportWwiseItem);
		}
		MenuBuilder.EndSection();
	}
	return MenuBuilder.MakeWidget();
}

SWwisePicker::~SWwisePicker()
{
	if (OnDatabaseUpdateCompleteHandle.IsValid())
	{
		FWwiseProjectDatabaseDelegates::Get().GetOnDatabaseUpdateCompletedDelegate().Remove(OnDatabaseUpdateCompleteHandle);
		OnDatabaseUpdateCompleteHandle.Reset();
	}
	RootItems.Empty();
}

void SWwisePicker::Construct(const FArguments& InArgs)
{

	FGenericCommands::Register();
	FWwisePickerViewCommands::Register();
	CreateWwisePickerCommands();

	SearchBoxFilter = MakeShareable( new StringFilter( StringFilter::FItemToStringArray::CreateSP( this, &SWwisePicker::PopulateSearchStrings ) ) );
	SearchBoxFilter->OnChanged().AddSP( this, &SWwisePicker::FilterUpdated );

    UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
    AkSettings->bRequestRefresh = true;
    
	ChildSlot
	[
		SNew(SBorder)
		.Padding(4)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SOverlay)

			// Picker
			+ SOverlay::Slot()
			.VAlign(VAlign_Fill)
			[
				SNew(SVerticalBox)

				// Search
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 1, 0, 3)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						InArgs._SearchContent.Widget
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SSearchBox)
						.HintText( LOCTEXT( "WwisePickerSearchTooltip", "Search Wwise Item" ) )
						.OnTextChanged( this, &SWwisePicker::OnSearchBoxChanged )
						.SelectAllTextWhenFocused(false)
						.DelayChangeNotificationsWhileTyping(true)
					]
				]

				// Tree title
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(3.0f)
					[
						SNew(SImage) 
						.Image(FAkAudioStyle::GetBrush(EWwiseItemType::Project))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0,0,3,0)
					[
						SNew(STextBlock)
						.Font( FEditorStyle::GetFontStyle("ContentBrowser.SourceTitleFont") )
						.Text( this, &SWwisePicker::GetProjectName )
						.Visibility(InArgs._ShowTreeTitle ? EVisibility::Visible : EVisibility::Collapsed)
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew( SSpacer )
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("AkPickerRefresh", "Refresh"))
						.OnClicked(this, &SWwisePicker::OnRefreshClicked)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("AkPickerGenerate", "Generate SoundBanks..."))
						.OnClicked(this, &SWwisePicker::OnGenerateSoundBanksClicked)
					]
				]

				// Separator
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 0, 0, 1)
				[
					SNew(SSeparator)
					.Visibility( ( InArgs._ShowSeparator) ? EVisibility::Visible : EVisibility::Collapsed )
				]
				
				// Tree
				+SVerticalBox::Slot()
				.FillHeight(1.f)
				[
					SAssignNew(TreeViewPtr, STreeView< TSharedPtr<FWwiseTreeItem> >)
					.TreeItemsSource(&RootItems).Visibility(this, &SWwisePicker::isWarningNotVisible)
					.OnGenerateRow( this, &SWwisePicker::GenerateRow )
					//.OnItemScrolledIntoView( this, &SPathView::TreeItemScrolledIntoView )
					.ItemHeight(18)
					.SelectionMode(InArgs._SelectionMode)
					.OnSelectionChanged(this, &SWwisePicker::TreeSelectionChanged)
					.OnExpansionChanged(this, &SWwisePicker::TreeExpansionChanged)
					.OnGetChildren( this, &SWwisePicker::GetChildrenForTree )
					.OnContextMenuOpening(this, &SWwisePicker::MakeWwisePickerContextMenu)
					//.OnSetExpansionRecursive( this, &SPathView::SetTreeItemExpansionRecursive )
					//.OnContextMenuOpening(this, &SPathView::MakePathViewContextMenu)
					.ClearSelectionOnClick(false)
				]
			]

			// Empty Picker
			+ SOverlay::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Visibility(this, &SWwisePicker::isWarningVisible)
				.AutoWrapText(true)
				.Justification(ETextJustify::Center)
				.Text(this, &SWwisePicker::GetWarningText)
				]
			+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoHeight()
				]
		]
	];

	InitialParse();
}

void SWwisePicker::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	UAkSettings* AkSettings = GetMutableDefault<UAkSettings>();
    if(AkSettings->bRequestRefresh)
    {
        ForceRefresh();
        AkSettings->bRequestRefresh = false;
    }
}

void SWwisePicker::ForceRefresh()
{
	DataLoader->UpdateWwiseTree();
	ConstructTree();
}

void SWwisePicker::InitialParse()
{
	ForceRefresh();
	TreeViewPtr->RequestTreeRefresh();
	ExpandFirstLevel();
}

FText SWwisePicker::GetProjectName() const
{
	auto* ProjectDatabase = UWwiseProjectDatabase::Get();
	if (UNLIKELY(!ProjectDatabase))
	{
		return {};
	}

	const FWwiseDataStructureScopeLock DataStructure(*ProjectDatabase);
	const auto Platform = DataStructure.GetPlatform(ProjectDatabase->GetCurrentPlatform());
	if(const auto* ProjectInfo = Platform.ProjectInfo.GetProjectInfo())
	{
		return FText::FromString(ProjectInfo->Project.Name);
	}
	return {};
}

EVisibility SWwisePicker::isWarningVisible() const
{
	return GetProjectName().IsEmpty() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SWwisePicker::isWarningNotVisible() const
{
	return GetProjectName().IsEmpty() ? EVisibility::Hidden : EVisibility::Visible;
}

FText SWwisePicker::GetWarningText() const
{
	FString soundBankDirectory = AkUnrealHelper::GetSoundBankDirectory();
	const FText WarningText = FText::FormatOrdered(LOCTEXT("MissingSoundBanks", "SoundBanks not found at path: {0}. Please make sure this is where Wwise generates them. If they are generated, click Refresh to force a refresh or accept to refresh the project when prompted."), FText::FromString(soundBankDirectory));
	
	return WarningText;
}

FReply SWwisePicker::OnRefreshClicked()
{
	if (FModuleManager::Get().IsModuleLoaded("AudiokineticTools"))
	{
		UE_LOG(LogAudiokineticTools, Verbose, TEXT("SWwisePicker::OnRefreshClicked: Reloading project data."));
		FModuleManager::Get().GetModuleChecked<IAudiokineticTools>(FName("AudiokineticTools")).RefreshWwiseProject();
	}
	ForceRefresh();
	return FReply::Handled();
}


FReply SWwisePicker::OnGenerateSoundBanksClicked()
{
	UE_LOG(LogAudiokineticTools, Verbose, TEXT("SWwisePicker::OnGenerateSoundBanksClicked: Opening Generate SoundBanks Window."));
	AkAudioBankGenerationHelper::CreateGenerateSoundDataWindow();
	return FReply::Handled();
}

void SWwisePicker::ConstructTree()
{
	RootItems.Empty(EWwiseItemType::LastWwisePickerType - EWwiseItemType::Event + 1);
	for (int i = EWwiseItemType::Event; i <= EWwiseItemType::LastWwisePickerType; ++i)
	{
		TSharedPtr<FWwiseTreeItem> NewRoot = DataLoader->GetTree(SearchBoxFilter, RootItems.Num() > i ? RootItems[i]: nullptr, static_cast<EWwiseItemType::Type>(i));
		RootItems.Add(NewRoot);
	}		

	RestoreTreeExpansion(RootItems);
	TreeViewPtr->RequestTreeRefresh();
}

void SWwisePicker::ExpandFirstLevel()
{
	// Expand root items and first-level work units.
	for(int32 i = 0; i < RootItems.Num(); i++)
	{
		TreeViewPtr->SetItemExpansion(RootItems[i], true);
	}
}

void SWwisePicker::ExpandParents(TSharedPtr<FWwiseTreeItem> Item)
{
	if(Item->Parent.IsValid())
	{
		ExpandParents(Item->Parent.Pin());
		TreeViewPtr->SetItemExpansion(Item->Parent.Pin(), true);
	}
}

TSharedRef<ITableRow> SWwisePicker::GenerateRow( TSharedPtr<FWwiseTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());

	auto RowVisibility = TreeItem->IsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	TSharedPtr<ITableRow> NewRow = SNew(STableRow< TSharedPtr<FWwiseTreeItem> >, OwnerTable)
		.OnDragDetected(this, &SWwisePicker::OnDragDetected)
		.Visibility(RowVisibility)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 1, 2, 1)
			.VAlign(VAlign_Center)
			[
				SNew(SImage) 
				.Image(FAkAudioStyle::GetBrush(TreeItem->ItemType))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TreeItem->DisplayName))
				.HighlightText(this, &SWwisePicker::GetHighlightText)
			]
		];

	TreeItem->TreeRow = NewRow;

	return NewRow.ToSharedRef();
}

void SWwisePicker::GetChildrenForTree(TSharedPtr< FWwiseTreeItem > TreeItem, TArray< TSharedPtr<FWwiseTreeItem> >& OutChildren)
{
	if (TreeItem)
	{
		OutChildren = TreeItem->GetChildren();
	}
}

FReply SWwisePicker::OnDragDetected(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	return DoDragDetected(MouseEvent, TreeViewPtr->GetSelectedItems());
}

FReply SWwisePicker::DoDragDetected(const FPointerEvent& MouseEvent, const TArray<TSharedPtr<FWwiseTreeItem>>& SelectedItems)
{
	if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Unhandled();
	}

	if (SelectedItems.Num() == 0)
	{
		return FReply::Unhandled();
	}

	UE_LOG(LogAudiokineticTools, Verbose, TEXT("SWwisePicker::OnDragDetected: User drag operation started."));

    auto AkSettings = GetMutableDefault<UAkSettings>();
	const FString DefaultPath = AkSettings->DefaultAssetCreationPath;

	// bool tracks whether the asset already existed. This is for proper cleanup after the drop operation.
	TMap<FAssetData, bool> NewAssets;
	bool bAssetExists;
	FAssetData AkAsset;
	TMultiMap<FName, FString> Search;
	FAssetData SearchResult;
	for (auto& Item : SelectedItems)
	{
		if (!Item->ItemId.IsValid() && !WwisePickerHelpers::IsFolder(Item))
		{
			UE_LOG(LogAudiokineticTools, Error, TEXT("Cannot drag selected Wwise asset: %s does not have a valid ID"), *(Item->FolderPath));
			continue;
		}

		bAssetExists = false;
		// If asset exists, then use that, else create a new one
		if (!AkAssetDatabase::Get().FindFirstAsset(Item->ItemId, SearchResult) || WwisePickerHelpers::IsFolder(Item))
		{
			TMap<UObject*, bool> NewWwiseAssets = WwisePickerHelpers::RecurseCreateAssets(Item, DefaultPath, "");
			if (NewWwiseAssets.Num() == 0)
			{
				UE_LOG(LogAudiokineticTools, Error, TEXT("Failed to create new Wwise asset %s from Picker"), *(Item->FolderPath));
				return FReply::Unhandled();
			}
			else
			{
				for (const TPair<UObject*, bool> NewAsset : NewWwiseAssets)
				{
					AkAsset = FAssetData(NewAsset.Key);
					NewAssets.Add(AkAsset, NewAsset.Value);
				}
			}
		}

		else
		{
			bAssetExists = true;
			AkAsset = SearchResult;
			AkAsset.AssetName = FName(Item->DisplayName);
			AkAsset.PackagePath = "";
			NewAssets.Add(AkAsset, bAssetExists);
		}
	}

	return FReply::Handled().BeginDragDrop(FWwiseAssetDragDropOp::New(NewAssets));
}

void SWwisePicker::ImportWwiseAssets(const TArray<TSharedPtr<FWwiseTreeItem>>& SelectedItems, const FString& PackagePath)
{
	UE_LOG(LogAudiokineticTools, Verbose, TEXT("SWwisePicker::ImportWwiseAssets: Creating %d assets in %s."), SelectedItems.Num(), *PackagePath);
	for(auto Asset: SelectedItems)
	{
		WwisePickerHelpers::RecurseCreateAssets(Asset, PackagePath, "");
	}
}

void SWwisePicker::PopulateSearchStrings(const FString& FolderName, OUT TArray< FString >& OutSearchStrings) const
{
	OutSearchStrings.Add(FolderName);
}

void SWwisePicker::OnSearchBoxChanged(const FText& InSearchText)
{
	SearchBoxFilter->SetRawFilterText(InSearchText);
}

FText SWwisePicker::GetHighlightText() const
{
	return SearchBoxFilter->GetRawFilterText();
}

void SWwisePicker::FilterUpdated()
{
	FScopedSlowTask SlowTask(2.f, LOCTEXT("AK_PopulatingPicker", "Populating Wwise Picker..."));
	SlowTask.MakeDialog();
	for (int32 i = 0; i < RootItems.Num(); i++)
	{
		RootItems[i] = DataLoader->GetTree(SearchBoxFilter, RootItems.Num() > i ? RootItems[i]: nullptr, static_cast<EWwiseItemType::Type>(i));

		AllowTreeViewDelegates = false;
		RestoreTreeExpansion(RootItems);
		AllowTreeViewDelegates = true;
	}
	TreeViewPtr->RequestTreeRefresh();
}

void SWwisePicker::SetItemVisibility(TSharedPtr<FWwiseTreeItem> Item, bool IsVisible)
{
	if (Item.IsValid())
	{
		if (IsVisible)
		{
			// Propagate visibility to parents.
			SetItemVisibility(Item->Parent.Pin(), IsVisible);
		}
		Item->IsVisible = IsVisible;
		if (Item->TreeRow.IsValid())
		{
			TSharedRef<SWidget> wid = Item->TreeRow.Pin()->AsWidget();
			wid->SetVisibility(IsVisible ? EVisibility::Visible : EVisibility::Collapsed);
		}
	}
}

void SWwisePicker::RestoreTreeExpansion(const TArray< TSharedPtr<FWwiseTreeItem> >& Items)
{
	for(int i = 0; i < Items.Num(); i++)
	{
		if (!Items[i])
		{
			continue;
		}

		if( LastExpandedPaths.Contains(Items[i]->FolderPath) )
		{
			TreeViewPtr->SetItemExpansion(Items[i], true);
		}
		RestoreTreeExpansion(Items[i]->GetChildren());
	}
}

void SWwisePicker::TreeSelectionChanged( TSharedPtr< FWwiseTreeItem > TreeItem, ESelectInfo::Type /*SelectInfo*/ )
{
	if (AllowTreeViewDelegates)
	{
		const TArray<TSharedPtr<FWwiseTreeItem>> SelectedItems = TreeViewPtr->GetSelectedItems();

		LastSelectedPaths.Empty();
		for (int32 ItemIdx = 0; ItemIdx < SelectedItems.Num(); ++ItemIdx)
		{
			const TSharedPtr<FWwiseTreeItem> Item = SelectedItems[ItemIdx];
			if (Item.IsValid())
			{
				LastSelectedPaths.Add(Item->FolderPath);
			}
		}
	}
}

void SWwisePicker::TreeExpansionChanged( TSharedPtr< FWwiseTreeItem > TreeItem, bool bIsExpanded )
{
	if (AllowTreeViewDelegates)
	{
		TSet<TSharedPtr<FWwiseTreeItem>> ExpandedItemSet;
		TreeViewPtr->GetExpandedItems(ExpandedItemSet);

		LastExpandedPaths.Empty();
		for (auto ExpandedItemIt = ExpandedItemSet.CreateConstIterator(); ExpandedItemIt; ++ExpandedItemIt)
		{
			const TSharedPtr<FWwiseTreeItem> Item = *ExpandedItemIt;
			if (Item.IsValid())
			{
				// Keep track of the last paths that we broadcasted for expansion reasons when filtering
				LastExpandedPaths.Add(Item->FolderPath);
			}
		}
	}
}

void SWwisePicker::HandleImportWwiseItemCommandExecute() const
{
	// If not prompting individual files, prompt the user to select a target directory.
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString LastWwiseImportPath = FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_IMPORT);
		FString FolderName;
		const FString Title = NSLOCTEXT("UnrealEd", "ChooseADirectory", "Choose A Directory").ToString();
		const bool bFolderSelected = DesktopPlatform->OpenDirectoryDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
			Title,
			LastWwiseImportPath,
			FolderName
		);

		if (bFolderSelected)
		{
			if (!FPaths::IsUnderDirectory(FolderName, FPaths::ProjectContentDir()))
			{

				const FText FailReason = FText::FormatOrdered(LOCTEXT("CannotImportWwiseItem", "Cannot import into {0}. Folder must be in content directory."), FText::FromString(FolderName));
				FMessageDialog::Open(EAppMsgType::Ok, FailReason);
				UE_LOG(LogAudiokineticTools, Error, TEXT("%s"), *FailReason.ToString());
				return;
			}
			FPaths::MakePathRelativeTo(FolderName, *FPaths::ProjectContentDir());
			const FString& PackagePath = TEXT("/Game")  / FolderName;
			FEditorDirectories::Get().SetLastDirectory(ELastDirectory::GENERIC_IMPORT, PackagePath);
			ImportWwiseAssets(TreeViewPtr->GetSelectedItems(), PackagePath);
		}
	}
}
#undef LOCTEXT_NAMESPACE
