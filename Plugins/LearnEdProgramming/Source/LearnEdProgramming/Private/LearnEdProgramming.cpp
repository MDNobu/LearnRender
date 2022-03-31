// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearnEdProgramming.h"
#include "LearnEdProgrammingStyle.h"
#include "LearnEdProgrammingCommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"

static const FName LearnEdProgrammingTabName("LearnEdProgramming");

#define LOCTEXT_NAMESPACE "FLearnEdProgrammingModule"

void FLearnEdProgrammingModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FLearnEdProgrammingStyle::Initialize();
	FLearnEdProgrammingStyle::ReloadTextures();

	FLearnEdProgrammingCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FLearnEdProgrammingCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FLearnEdProgrammingModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FLearnEdProgrammingModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(LearnEdProgrammingTabName, FOnSpawnTab::CreateRaw(this, &FLearnEdProgrammingModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FLearnEdProgrammingTabTitle", "LearnEdProgramming"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FLearnEdProgrammingModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FLearnEdProgrammingStyle::Shutdown();

	FLearnEdProgrammingCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(LearnEdProgrammingTabName);
}

TSharedRef<SDockTab> FLearnEdProgrammingModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	FText WidgetText = FText::Format(
		LOCTEXT("WindowWidgetText", "Add code to {0} in {1} to override this window's contents"),
		FText::FromString(TEXT("FLearnEdProgrammingModule::OnSpawnPluginTab")),
		FText::FromString(TEXT("LearnEdProgramming.cpp"))
		);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			// Put your tab content here!
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(WidgetText)
			]
		];
}

void FLearnEdProgrammingModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(LearnEdProgrammingTabName);
}

void FLearnEdProgrammingModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FLearnEdProgrammingCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("Settings");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FLearnEdProgrammingCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLearnEdProgrammingModule, LearnEdProgramming)