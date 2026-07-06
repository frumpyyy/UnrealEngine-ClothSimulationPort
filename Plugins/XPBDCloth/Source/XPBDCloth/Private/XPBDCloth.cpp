// Copyright Epic Games, Inc. All Rights Reserved.

#include "XPBDCloth.h"

#if WITH_EDITOR
#include "IPlacementModeModule.h"
#endif

#define LOCTEXT_NAMESPACE "FXPBDClothModule"

void FXPBDClothModule::StartupModule()
{

#if WITH_EDITOR
	if (IPlacementModeModule::IsAvailable()) {
		FPlacementCategoryInfo placementInfo(
			FText::FromString("Cloth"),
			FSlateIcon("EditorStyle", "PlacementBrowser.Icons.All"),
			"XPBDCloth",
			TEXT("PMXPBDCloth"), 30);

		IPlacementModeModule::Get().RegisterPlacementCategory(placementInfo);
	}
#endif

}

void FXPBDClothModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FXPBDClothModule, XPBDCloth)