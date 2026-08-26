// Copyright 2019 UNAmedia. All Rights Reserved.

#include "StereoPanoramicPlayer.h"

#if WITH_EDITOR
#include "PropertyEditorModule.h"
#include "PanoramicStageCustomization.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "FStereoPanoramicPlayerModule"



DEFINE_LOG_CATEGORY(LogStereoPanoramicPlayer)



void FStereoPanoramicPlayerModule::StartupModule()
{
#if WITH_EDITOR
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.RegisterCustomClassLayout("PanoramicStage", 
        FOnGetDetailCustomizationInstance::CreateStatic(
            &FPanoramicStageCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout("PanoramicStageNavigation",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(
            &FPanoramicStageNavigationCustomization::MakeInstance));

    PropertyModule.RegisterCustomPropertyTypeLayout("PanoramicStageMovieData",
        FOnGetPropertyTypeCustomizationInstance::CreateStatic(
            &FPanoramicStageMovieDataCustomization::MakeInstance));
#endif // WITH_EDITOR
}

void FStereoPanoramicPlayerModule::ShutdownModule()
{
#if WITH_EDITOR
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

    PropertyModule.UnregisterCustomClassLayout("PanoramicStage");
    PropertyModule.UnregisterCustomPropertyTypeLayout("PanoramicStageNavigation");
    PropertyModule.UnregisterCustomPropertyTypeLayout("PanoramicStageMovieData");
#endif // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FStereoPanoramicPlayerModule, StereoPanoramicPlayer)