#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FVulkanVideoMeshComponentModule"

class FVulkanVideoMeshComponentModule : public IModuleInterface
{

    void StartupModule()
    {
        // This code will execute after your module is loaded into memory; the exact timing is
        // specified in the .uplugin file per-module

        // Get the base directory of this plugin
        //	FString BaseDir = IPluginManager::Get().FindPlugin("AndroidVulkanVideo")->GetBaseDir();
        //
    }

    void ShutdownModule()
    {
        // This function may be called during shutdown to clean up your module.  For modules that
        // support dynamic reloading, we call this function before unloading the module.
    }
};
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVulkanVideoMeshComponentModule, VulkanVideoMeshComponent)
