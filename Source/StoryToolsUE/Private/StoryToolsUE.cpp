// Copyright Epic Games, Inc. All Rights Reserved.

#include "StoryToolsUE.h"
#include "StoryToolsUEStyle.h"
#include "StoryToolsUECommands.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "ToolMenus.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SBorder.h"
#include "FileHelpers.h"
#include "Selection.h"

static const FName StoryToolsUETabName("StoryToolsUE");

DEFINE_LOG_CATEGORY_STATIC(LogStoryToolsUE, Log, All);

#define LOCTEXT_NAMESPACE "FStoryToolsUEModule"

void FStoryToolsUEModule::StartupModule()
{
	FStoryToolsUEStyle::Initialize();
	FStoryToolsUEStyle::ReloadTextures();

	FStoryToolsUECommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FStoryToolsUECommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FStoryToolsUEModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FStoryToolsUEModule::RegisterMenus));

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StoryToolsUETabName, FOnSpawnTab::CreateRaw(this, &FStoryToolsUEModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FStoryToolsUETabTitle", "StoryToolsUE"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FStoryToolsUEModule::ShutdownModule()
{
	bIsShuttingDown = true;

	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FStoryToolsUEStyle::Shutdown();
	FStoryToolsUECommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StoryToolsUETabName);
}

void FStoryToolsUEModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(StoryToolsUETabName);
}

void FStoryToolsUEModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FStoryToolsUECommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("StoryPluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FStoryToolsUECommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

// ================= 路径辅助 =================

FString FStoryToolsUEModule::GetStoryDir() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("../story"));
}

FString FStoryToolsUEModule::GetStoryToolsDir() const
{
	return GetStoryDir() / TEXT("StoryTools");
}

FString FStoryToolsUEModule::GetConfigJsonPath() const
{
	return GetStoryDir() / TEXT("config.json");
}

FString FStoryToolsUEModule::GetStoryTextDir() const
{
	return GetStoryDir() / TEXT("story");
}

// ================= Python 检测 =================

bool FStoryToolsUEModule::IsPythonAvailable(FString& OutVersion) const
{
	uint32 ProcessId = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		TEXT("python"),
		TEXT("--version"),
		false,
		false,
		false,
		&ProcessId,
		0,
		nullptr,
		nullptr,
		nullptr
	);

	bool bAvailable = ProcHandle.IsValid();
	if (bAvailable)
	{
		FPlatformProcess::CloseProc(ProcHandle);
		OutVersion = TEXT("Python 已安装");
	}
	else
	{
		OutVersion = TEXT("未检测到 Python");
	}

	return bAvailable;
}

void FStoryToolsUEModule::RefreshTextFileList()
{
	TextFileOptions.Empty();
	SelectedTextFile.Reset();

	const FString TextDir = GetStoryTextDir();
	IFileManager::Get().MakeDirectory(*TextDir, true);

	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(TextDir / TEXT("*.txt")), true, false);

	Files.Sort();

	for (const FString& File : Files)
	{
		TextFileOptions.Add(MakeShared<FString>(File));
	}

	if (TextFileOptions.Num() > 0)
	{
		SelectedTextFile = TextFileOptions[0];
	}

	// 若 ComboBox 已创建，通知刷新
	if (TextFileComboBox.IsValid())
	{
		TextFileComboBox->RefreshOptions();
		if (SelectedTextFile.IsValid())
		{
			TextFileComboBox->SetSelectedItem(SelectedTextFile);
		}
	}
}

void FStoryToolsUEModule::OnTextFileSelected(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo == ESelectInfo::Direct)
	{
		// 程序内部赋值，无需处理
	}
	SelectedTextFile = NewSelection;
}

FReply FStoryToolsUEModule::HandleNewTextFileClicked()
{
	TSharedPtr<SEditableTextBox> NameInput = SNew(SEditableTextBox)
		.HintText(FText::FromString(TEXT("输入文件名（不含 .txt）")))
		.MinDesiredWidth(240.f);

	TSharedPtr<SWindow> DialogWindow = SNew(SWindow)
		.Title(FText::FromString(TEXT("新建故事文本")))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	bool bConfirmed = false;

	DialogWindow->SetContent(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
		[
			SNew(STextBlock).Text(FText::FromString(TEXT("文件名：")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f)
		[
			NameInput.ToSharedRef()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(8.f).HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("确定")))
						.OnClicked_Lambda([DialogWindow, &bConfirmed]() -> FReply
							{
								bConfirmed = true;
								DialogWindow->RequestDestroyWindow();
								return FReply::Handled();
							})
				]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("取消")))
						.OnClicked_Lambda([DialogWindow, &bConfirmed]() -> FReply
							{
								bConfirmed = false;
								DialogWindow->RequestDestroyWindow();
								return FReply::Handled();
							})
				]
		]
	);

	FSlateApplication::Get().AddModalWindow(DialogWindow.ToSharedRef(), nullptr);

	if (!bConfirmed)
	{
		return FReply::Handled();
	}

	FString FileName = NameInput->GetText().ToString().TrimStartAndEnd();
	if (FileName.IsEmpty())
	{
		UE_LOG(LogStoryToolsUE, Warning, TEXT("[新建 txt] 文件名为空，已取消。"));
		return FReply::Handled();
	}

	// 自动补 .txt 后缀
	if (!FileName.EndsWith(TEXT(".txt"), ESearchCase::IgnoreCase))
	{
		FileName += TEXT(".txt");
	}

	// 过滤非法字符
	FileName = FPaths::MakeValidFileName(FileName);

	const FString TextDir = GetStoryTextDir();
	IFileManager::Get().MakeDirectory(*TextDir, true);

	const FString FullPath = TextDir / FileName;

	if (FPaths::FileExists(FullPath))
	{
		UE_LOG(LogStoryToolsUE, Warning, TEXT("[新建 txt] 文件已存在：%s"), *FullPath);
		return FReply::Handled();
	}

	FFileHelper::SaveStringToFile(
		TEXT(""),
		*FullPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	RefreshTextFileList();

	// 选中新建的文件
	for (TSharedPtr<FString>& Item : TextFileOptions)
	{
		if (Item.IsValid() && *Item == FileName)
		{
			SelectedTextFile = Item;
			if (TextFileComboBox.IsValid())
			{
				TextFileComboBox->SetSelectedItem(SelectedTextFile);
			}
			break;
		}
	}

	return FReply::Handled();
}

FReply FStoryToolsUEModule::HandleOpenSelectedTextClicked()
{
	if (!SelectedTextFile.IsValid() || SelectedTextFile->IsEmpty())
	{
		UE_LOG(LogStoryToolsUE, Warning, TEXT("[打开 txt] 未选择任何文件。"));
		return FReply::Handled();
	}

	const FString FullPath = GetStoryTextDir() / *SelectedTextFile;

	if (!FPaths::FileExists(FullPath))
	{
		UE_LOG(LogStoryToolsUE, Warning, TEXT("[打开 txt] 文件不存在：%s"), *FullPath);
		RefreshTextFileList();
		return FReply::Handled();
	}

	FPlatformProcess::LaunchFileInDefaultExternalApplication(*FullPath);
	return FReply::Handled();
}

static void RunCommandHidden(const FString& InExe, const FString& InArgs, const FString& InWorkingDir, TFunction<void()> OnFinished)
{
	void* ReadPipe = nullptr;
	void* WritePipe = nullptr;
	FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

	uint32 ProcessId = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*InExe,
		InArgs.IsEmpty() ? nullptr : *InArgs,
		true, true, true,
		&ProcessId,
		0,
		InWorkingDir.IsEmpty() ? nullptr : *InWorkingDir,
		WritePipe,
		nullptr,
		nullptr
	);

	if (!ProcHandle.IsValid())
	{
		UE_LOG(LogStoryToolsUE, Warning, TEXT("[RunCommandHidden] CreateProc FAILED. Exe=%s Args=%s Dir=%s"), *InExe, *InArgs, *InWorkingDir);
		FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
		if (OnFinished) OnFinished();
		return;
	}
	UE_LOG(LogStoryToolsUE, Warning, TEXT("[RunCommandHidden] Proc started. PID=%u Exe=%s"), ProcessId, *InExe);

	struct FProcWatcher
	{
		FProcHandle Handle;
		void* ReadPipe;
		void* WritePipe;
		FString OutputAccum;
		TFunction<void()> Callback;
	};
	TSharedPtr<FProcWatcher> Watcher = MakeShared<FProcWatcher>();
	Watcher->Handle = ProcHandle;
	Watcher->ReadPipe = ReadPipe;
	Watcher->WritePipe = WritePipe;
	Watcher->Callback = MoveTemp(OnFinished);

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([Watcher](float) -> bool
			{
				if (!Watcher.IsValid())
				{
					return true;
				}

				TArray<uint8> Buffer;
				while (FPlatformProcess::ReadPipeToArray(Watcher->ReadPipe, Buffer))
				{
					if (Buffer.Num() == 0) break;
					FString Str = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buffer.GetData())));
					UE_LOG(LogStoryToolsUE, Log, TEXT("%s"), *Str);
					Buffer.Reset();
				}

				if (!FPlatformProcess::IsProcRunning(Watcher->Handle))
				{
					FString Tail = FPlatformProcess::ReadPipe(Watcher->ReadPipe);
					if (!Tail.IsEmpty())
					{
						TArray<FString> Lines;
						Tail.ParseIntoArrayLines(Lines);
						for (const FString& Line : Lines)
						{
							if (!Line.IsEmpty())
								UE_LOG(LogStoryToolsUE, Warning, TEXT("%s"), *Line);
						}
					}

					UE_LOG(LogStoryToolsUE, Warning, TEXT("[RunCommandHidden] 进程已结束。PID=%u"), Watcher->Handle.Get());

					FPlatformProcess::CloseProc(Watcher->Handle);
					FPlatformProcess::ClosePipe(Watcher->ReadPipe, Watcher->WritePipe);
					TFunction<void()> Cb = MoveTemp(Watcher->Callback);
					if (Cb) Cb();
					return false;
				}
				return true;
			}),
		0.2f
	);
}

FReply FStoryToolsUEModule::HandlePythonWebsiteClicked()
{
	FPlatformProcess::LaunchURL(TEXT("https://www.python.org/downloads/"), nullptr, nullptr);
	return FReply::Handled();
}

FReply FStoryToolsUEModule::HandleGitCloneClicked()
{
	const FString StoryDir = GetStoryDir();
	if (!FPaths::DirectoryExists(StoryDir))
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*StoryDir);
	}

	FStoryToolsUEModule* RawThis = this;
	RunCommandHidden(
		TEXT("git.exe"),
		TEXT("clone --progress https://github.com/PeacefulWindy/StoryToolsUE.git"),
		StoryDir,
		[RawThis]()
		{
			if (RawThis && !RawThis->bIsShuttingDown)
			{
				RawThis->RefreshUI();
			}
		}
	);
	return FReply::Handled();
}

FReply FStoryToolsUEModule::HandleOpenConfigJsonClicked()
{
	const FString ConfigJsonPath = GetConfigJsonPath();

	if (!FPaths::FileExists(ConfigJsonPath))
	{
		const FString StoryDir = GetStoryDir();
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*StoryDir);

		const FString DefaultJson = TEXT(
			"{\n"
			"    \"input\":\"../story\",\n"
			"    \"output\":\"../output/StoryScript.xlsx\",\n"
			"    \"move\":\"../../configs/excel/StoryScript.xlsx\"\n"
			"}\n"
		);
		FFileHelper::SaveStringToFile(DefaultJson, *ConfigJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FPlatformProcess::LaunchFileInDefaultExternalApplication(*ConfigJsonPath);

	return FReply::Handled();
}

FReply FStoryToolsUEModule::HandleRunGenerateClicked()
{
	const FString StoryToolsDir = GetStoryToolsDir();
	if (!FPaths::DirectoryExists(StoryToolsDir))
	{
		return FReply::Handled();
	}

	RunCommandHidden(
		TEXT("python.exe"),
		TEXT("generate.py ../config.json -once"),
		StoryToolsDir,
		nullptr
	);

	return FReply::Handled();
}

// ================= UI =================

void FStoryToolsUEModule::BuildContent()
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	FString Version;
	const bool bPythonOk = IsPythonAvailable(Version);
	const bool bStoryToolsExists = FPaths::DirectoryExists(GetStoryToolsDir());
	const bool bConfigJsonExists = FPaths::FileExists(GetConfigJsonPath());

	// Python 状态行
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(bPythonOk ? TEXT("✔ Python 已安装") : TEXT("✘ 未检测到 Python")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0, 0, 0).VAlign(VAlign_Center)
				[
					SNew(SButton)
						.Text(FText::FromString(bPythonOk ? TEXT("重新检查") : TEXT("前往 Python 官网下载")))
						.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandlePythonWebsiteClicked))
						.Visibility(bPythonOk ? EVisibility::Collapsed : EVisibility::Visible)
				]
		];

	// StoryToolsUE 仓库状态行
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock)
						.Text(FText::FromString(bStoryToolsExists ? TEXT("✔ StoryToolsUE 已下载") : TEXT("✘ StoryToolsUE 不存在")))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0, 0, 0)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("从 GitHub 克隆")))
						.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandleGitCloneClicked))
						.Visibility(bStoryToolsExists ? EVisibility::Collapsed : EVisibility::Visible)
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(12.f, 0, 0, 0)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("打开 config.json")))
						.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandleOpenConfigJsonClicked))
						.Visibility(bStoryToolsExists ? EVisibility::Visible : EVisibility::Collapsed)
				]
		];

	// 执行按钮
	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SButton)
				.Text(FText::FromString(TEXT("执行 generate.py")))
				.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandleRunGenerateClicked))
				.IsEnabled(bStoryToolsExists && bPythonOk)
		];

	// ============ 故事文本管理 ============

	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 12.f, 8.f, 4.f)
		[
			SNew(SSeparator)
		];

	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(STextBlock)
				.Text(FText::FromString(TEXT("故事文本管理 (../story/story/*.txt)")))
		];

	// 扫描目录，填充 TextFileOptions / SelectedTextFile
	// 注意：此时 TextFileComboBox 可能还是旧指针或空，RefreshTextFileList 内部
	// 会根据 TextFileComboBox.IsValid() 决定是否通知刷新，安全。
	TextFileComboBox.Reset();  // 重建时先清空旧指针，避免 RefreshTextFileList 操作已失效控件
	RefreshTextFileList();

	ContentBox->AddSlot()
		.AutoHeight()
		.Padding(8.f, 4.f)
		[
			SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0, 0, 8.f, 0)
				[
					SNew(SBox)
						.MinDesiredWidth(260.f)
						[
							SAssignNew(TextFileComboBox, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&TextFileOptions)
								.InitiallySelectedItem(SelectedTextFile)
								.OnSelectionChanged(SComboBox<TSharedPtr<FString>>::FOnSelectionChanged::CreateRaw(this, &FStoryToolsUEModule::OnTextFileSelected))
								.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem)
									{
										return SNew(STextBlock)
											.Text(InItem.IsValid() ? FText::FromString(*InItem) : FText::GetEmpty());
									})
								[
									SNew(STextBlock)
										.Text_Lambda([this]()
											{
												return (SelectedTextFile.IsValid() && !SelectedTextFile->IsEmpty())
													? FText::FromString(*SelectedTextFile)
													: FText::FromString(TEXT("<无文本文件>"));
											})
								]
						]
				]
			+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("新建 txt")))
						.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandleNewTextFileClicked))
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0)
				[
					SNew(SButton)
						.Text(FText::FromString(TEXT("打开选中")))
						.OnClicked(FOnClicked::CreateRaw(this, &FStoryToolsUEModule::HandleOpenSelectedTextClicked))
						.IsEnabled_Lambda([this]()
							{
								return SelectedTextFile.IsValid() && !SelectedTextFile->IsEmpty();
							})
				]
		];
}

void FStoryToolsUEModule::RefreshUI()
{
	if (!ContentBox.IsValid())
	{
		return;
	}
	ContentBox->ClearChildren();
	BuildContent();
}

TSharedRef<SDockTab> FStoryToolsUEModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	ContentBox = SNew(SVerticalBox);
	BuildContent();

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Top)
				.Padding(16.f)
				[
					ContentBox.ToSharedRef()
				]
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FStoryToolsUEModule, StoryToolsUE)