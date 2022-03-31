// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearnEdProgrammingCommands.h"

#define LOCTEXT_NAMESPACE "FLearnEdProgrammingModule"

void FLearnEdProgrammingCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "LearnEdProgramming", "Bring up LearnEdProgramming window", EUserInterfaceActionType::Button, FInputGesture());
}

#undef LOCTEXT_NAMESPACE
