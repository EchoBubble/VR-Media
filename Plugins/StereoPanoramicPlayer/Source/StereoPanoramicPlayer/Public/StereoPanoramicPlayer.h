// Copyright 2019 UNAmedia. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogStereoPanoramicPlayer, Warning, All)

/**
 * Plugin module implementation
 */
class FStereoPanoramicPlayerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;

    /** IModuleInterface implementation */
	virtual void ShutdownModule() override;
};
