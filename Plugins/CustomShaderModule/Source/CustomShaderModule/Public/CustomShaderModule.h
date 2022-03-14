// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCustomShaderModuleModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FCustomShaderModuleModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FCustomShaderModuleModule>("CustomShaderModule");
	}
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("CustomShaderModule");
	}
};
