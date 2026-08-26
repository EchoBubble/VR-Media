// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Vulkan video player module. Handles starting up
// the correct vulkan extensions, and enabling
// the override layer which we use to enable
// SamplerYCbCrConversion feature properly.
// ------------------------------------------------

#include "AndroidVulkanVideo.h"
// #include "Core.h"
#include "Modules/ModuleManager.h"
// #include "Interfaces/IPluginManager.h"
#if PLATFORM_ANDROID
#include "AndroidVulkanMediaPlayer.h"
#include "IVulkanDynamicRHI.h"
#endif

#define LOCTEXT_NAMESPACE "FAndroidVulkanVideoModule"

class FAndroidVulkanVideoModule : public IAndroidVulkanVideoModule
{

    void StartupModule()
    {
        // This code will execute after your module is loaded into memory; the exact timing is
        // specified in the .uplugin file per-module

        // Get the base directory of this plugin
//	FString BaseDir = IPluginManager::Get().FindPlugin("AndroidVulkanVideo")->GetBaseDir();
//
#if PLATFORM_ANDROID
        // setup vulkan extensions that we need
        const TArray<const ANSICHAR *> DeviceExtensions = {
            "VK_KHR_external_memory",
            "VK_ANDROID_external_memory_android_hardware_buffer",
            "VK_KHR_sampler_ycbcr_conversion",
            "VK_KHR_bind_memory2",
            "VK_EXT_queue_family_foreign",
            "VK_KHR_dedicated_allocation",
            "VK_KHR_get_physical_device_properties2"};
        const TArray<const ANSICHAR *> DeviceLayers = {};

        const TArray<const ANSICHAR *> InstanceExtensions = {
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME};
        const TArray<const ANSICHAR *> InstanceLayers = {"VkLayer_DirectVideo_"
                                                         "23"};

        IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(DeviceExtensions, DeviceLayers);
        IVulkanDynamicRHI::AddEnabledInstanceExtensionsAndLayers(InstanceExtensions,
                                                                 InstanceLayers);
#endif
    }

    void ShutdownModule()
    {
        // This function may be called during shutdown to clean up your module.  For modules that
        // support dynamic reloading, we call this function before unloading the module.

        // Free the dll handle
    }

    TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(IMediaEventSink &EventSink)
    {
        return TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe>(
            new FAndroidVulkanMediaPlayer(EventSink));
    }
};
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAndroidVulkanVideoModule, AndroidVulkanVideo)
