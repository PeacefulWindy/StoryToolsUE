// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "StoryToolsUEStyle.h"

class FStoryToolsUECommands : public TCommands<FStoryToolsUECommands>
{
public:

	FStoryToolsUECommands()
		: TCommands<FStoryToolsUECommands>(TEXT("StoryToolsUE"), NSLOCTEXT("Contexts", "StoryToolsUE", "StoryToolsUE Plugin"), NAME_None, FStoryToolsUEStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};