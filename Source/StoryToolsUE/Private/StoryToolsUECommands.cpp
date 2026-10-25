// Copyright Epic Games, Inc. All Rights Reserved.

#include "StoryToolsUECommands.h"

#define LOCTEXT_NAMESPACE "FStoryToolsUEModule"

void FStoryToolsUECommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "StoryToolsUE", "StoryToolsUE window", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE
