// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// AndroidVulkanVideo module implementation
// ------------------------------------------------

#pragma once

#include "Modules/ModuleManager.h"

#include "IMediaEventSink.h"
#include "IMediaPlayer.h"

class IAndroidVulkanVideoModule : public IModuleInterface
{
  public:
    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(
        IMediaEventSink &EventSink) = 0;

  private:
};
