// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Callback used by implementation class to send
// buffers to media player.
// ------------------------------------------------
#pragma once
class IAndroidVulkanVideoAVCallback
{
  public:
    virtual ~IAndroidVulkanVideoAVCallback()
    {
    }
    // called with video frame hardware buffers
    virtual void onVideoFrame(void *frameHwBuffer, int w, int h, int64_t presTimeNs) = 0;
    // we need to get vulkan functions and device etc. via Unreal Engine for consistency
    virtual void *getVkDeviceProcAddr(const char *name) = 0;
    virtual VkDevice getVkDevice() = 0;
    virtual const VkAllocationCallbacks *getVkAllocationCallbacks() = 0;
    virtual VkPhysicalDevice getNativePhysicalDevice() = 0;

    virtual void onPlaybackEnd(bool looping) = 0;
};
