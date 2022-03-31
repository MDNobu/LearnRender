// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "LearnEdProgrammingStyle.h"

class FLearnEdProgrammingCommands : public TCommands<FLearnEdProgrammingCommands>
{
public:

	FLearnEdProgrammingCommands()
		: TCommands<FLearnEdProgrammingCommands>(TEXT("LearnEdProgramming"), NSLOCTEXT("Contexts", "LearnEdProgramming", "LearnEdProgramming Plugin"), NAME_None, FLearnEdProgrammingStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};