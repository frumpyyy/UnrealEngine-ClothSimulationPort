// Copyright Epic Games, Inc. All Rights Reserved.

#include "XPBDClothShaders.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FXPBDClothShadersModule"

void FXPBDClothShadersModule::StartupModule()
{
	FString pluginDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("XPBDCloth"))->GetBaseDir(), TEXT("Shaders/Private"));

	AddShaderSourceDirectoryMapping(TEXT("/XPBDCloth"), pluginDir);
}

void FXPBDClothShadersModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXPBDClothShadersModule, XPBDClothShaders)