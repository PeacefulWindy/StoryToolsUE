// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FStoryToolsUEModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void PluginButtonClicked();

private:
	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	void BuildContent();
	void RefreshUI();

	FString GetStoryDir() const;
	FString GetStoryToolsDir() const;
	FString GetConfigJsonPath() const;
	FString GetStoryTextDir() const;

	bool IsPythonAvailable(FString& OutVersion) const;

	void RefreshTextFileList();
	void OnTextFileSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);

	FReply HandlePythonWebsiteClicked();
	FReply HandleGitCloneClicked();
	FReply HandleOpenConfigJsonClicked();
	FReply HandleRunGenerateClicked();
	FReply HandleNewTextFileClicked();
	FReply HandleOpenSelectedTextClicked();

private:
	TSharedPtr<class FUICommandList> PluginCommands;
	TSharedPtr<SVerticalBox> ContentBox;
	bool bIsShuttingDown = false;

	TArray<TSharedPtr<FString>> TextFileOptions;
	TSharedPtr<FString> SelectedTextFile;
	TSharedPtr<class SComboBox<TSharedPtr<FString>>> TextFileComboBox;
};