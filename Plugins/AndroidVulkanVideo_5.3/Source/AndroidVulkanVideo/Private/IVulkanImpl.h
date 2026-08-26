// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Interface used by Android specific implementation class
// This does all handling of Android hardware
// buffers, Vulkan texture images etc.
//
// It lives in libVkLayer_DirectVideo_version.so
// along with a vulkan override layer to handle
// initialisation of the Vulkan device.
//
// ------------------------------------------------
#pragma once

#include "IAndroidVulkanVideoAVCallback.h"
#include "IAudioOut.h"
#include "ICustomMediaFileSource.h"

#include <vulkan/vulkan.h>

class ILogger;
#include "IVulkanVertexData.h"

class IVulkanImpl
{
  public:
    enum StereoMode
    {
        STEREO_NONE,
        STEREO_SBS,
        STEREO_TB
    };

    virtual ~IVulkanImpl()
    {
    }

    virtual bool initializeFrameObjectsOutsidePass(VkCommandBuffer cmdBuffer, void *frameImage,
                                                   VkImage outImage, int w, int h, void *meshID,
                                                   int samples, int drawViews) = 0;
    virtual bool captureMainRenderPassID(VkCommandBuffer cmdBuffer) = 0;
    virtual bool setMeshCullmode(VkCullModeFlagBits cullMode) = 0;
    virtual bool renderFrameToMeshInExistingPass(VkCommandBuffer cmdBuffer, void *frameImage,
                                                 VkImage outImage, int x, int y, int w, int h,
                                                 int samples, int drawViews, void *meshID) = 0;
    virtual bool updateViewMatrices(VkCommandBuffer cmdBuffer, const ShaderViewMatrices &matrices,
                                    VkImage targetImage) = 0;
    virtual bool updateModelMatrix(VkCommandBuffer cmdBuffer, const ShaderModelMatrix &matrix,
                                   VkImage targetImage, void *meshID) = 0;
    virtual bool loadMesh(VkCommandBuffer cmdBuffer, const VertexData *positionAndUV,
                          int vertexCount, const uint32_t *indices, int indexCount,
                          void *meshID) = 0;
    virtual bool hasMesh(void *meshID) const = 0;
    virtual bool unloadMesh(void *meshID) = 0;

    virtual bool isReadyToRender(void *frameImage) = 0;

    virtual bool renderFrameToImageView(VkCommandBuffer cmdBuffer, void *frameImage,
                                        VkImageView view, VkImage image, VkFormat format, int w,
                                        int h, int samples) = 0;
    virtual void releaseFrame(void *frameHwBuffer) = 0;

    virtual void setDataCallback(IAndroidVulkanVideoAVCallback *cb) = 0;
    virtual void setLogger(ILogger *logger) = 0;

    virtual bool startVideoCustomSource(ICustomMediaFileSource *source, bool autoplay) = 0;
    virtual bool startVideoURL(char *videoURL, bool autoplay) = 0;
    virtual bool startVideoFile(char *fname, int64_t offset, int64_t length, bool autoplay) = 0;
    virtual bool hasVideo(int *width, int *height) = 0;

    virtual bool getVideoTrackFormat(int idx, int *width, int *height, float *fps) = 0;
    virtual bool getAudioTrackFormat(int idx, int *bitsPerSample, int *channels, int *rate) = 0;

    // set audio out handler - typically has to be done once per
    // run, as it is re-initialized per video
    virtual void setAudioOut(IAudioOut *audioOut) = 0;

    // set the output texture
    virtual void setOutputImageView(VkImageView view, VkImage image, VkFormat format, int w, int h,
                                    int samples) = 0;

    virtual void close() = 0;

    virtual void setPlaying(bool playing) = 0;
    virtual bool getPlaying() const = 0;
    virtual void seek(int64_t nsTimestamp) = 0;
    virtual bool setRate(float rate) = 0;
    virtual float getRate() const = 0;
    virtual int64_t getTimeNS() const = 0;
    virtual int64_t getDurationNS() const = 0;
    virtual int numAudioTracks() const = 0;
    virtual int numVideoTracks() const = 0;
    virtual bool selectAudioTrack(int idx) = 0;
    virtual bool selectVideoTrack(int idx) = 0;
    virtual int getSelectedVideoTrack() = 0;
    virtual int getSelectedAudioTrack() = 0;

    virtual void setLooping(bool looping) = 0;
    virtual bool getLooping() const = 0;

    virtual void setStereoMode(StereoMode mode) = 0;
};
