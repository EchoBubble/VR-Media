// ------------------------------------------------
// Copyright Joe Marshall 2024- All Rights Reserved
// ------------------------------------------------
//
// Factory class to tell Unreal that we can play
// different types of media.
// ------------------------------------------------

#include "AndroidVulkanVideo.h"
#include "IMediaModule.h"
#include "IMediaPlayerFactory.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAndroidVulkanVideoFactoryModule"

class FAndroidVulkanVideoFactoryModule : public IMediaPlayerFactory, public IModuleInterface
{
  public:
    virtual bool CanPlayUrl(const FString &Url, const IMediaOptions * /*Options*/,
                            TArray<FText> * /*OutWarnings*/,
                            TArray<FText> *OutErrors) const override
    {
        FString Scheme;
        FString Location;

        // check scheme
        if (!Url.Split(TEXT("://"), &Scheme, &Location, ESearchCase::CaseSensitive))
        {
            if (OutErrors != nullptr)
            {
                OutErrors->Add(LOCTEXT("NoSchemeFound", "No URI scheme found"));
            }

            return false;
        }

        if (!SupportedUriSchemes.Contains(Scheme))
        {
            if (OutErrors != nullptr)
            {
                OutErrors->Add(FText::Format(
                    LOCTEXT("SchemeNotSupported", "The URI scheme '{0}' is not supported"),
                    FText::FromString(Scheme)));
            }

            return false;
        }

        // check file extension
        if (Scheme == TEXT("file"))
        {
            const FString Extension = FPaths::GetExtension(Location, false);

            if (!SupportedFileExtensions.Contains(Extension))
            {
                if (OutErrors != nullptr)
                {
                    OutErrors->Add(
                        FText::Format(LOCTEXT("ExtensionNotSupported",
                                              "The file extension '{0}' is not supported"),
                                      FText::FromString(Extension)));
                }

                return false;
            }
        }

        return true;
    }

    virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer(
        IMediaEventSink &EventSink) override
    {
        auto AndroidVulkanVideoModule =
            FModuleManager::LoadModulePtr<IAndroidVulkanVideoModule>("AndroidVulkanVideo");
        return (AndroidVulkanVideoModule != nullptr)
                   ? AndroidVulkanVideoModule->CreatePlayer(EventSink)
                   : nullptr;
    }

    virtual FText GetDisplayName() const override
    {
        return LOCTEXT("MediaPlayerDisplayName", "Android Vulkan Video");
    }

    virtual FName GetPlayerName() const override
    {
        static FName PlayerName(TEXT("DirectVideo Android"));
        return PlayerName;
    }

    virtual FGuid GetPlayerPluginGUID() const override
    {
        static FGuid OurGUID(0x9bf2d7c6, 0xb2b84d26, 0xb6ae5a3a, 0xc9883569);
        return OurGUID;
    }

    virtual const TArray<FString> &GetSupportedPlatforms() const override
    {
        return SupportedPlatforms;
    }

    virtual bool SupportsFeature(EMediaFeature Feature) const override
    {
        return ((Feature == EMediaFeature::AudioTracks) ||
                (Feature == EMediaFeature::VideoSamples) ||
                (Feature == EMediaFeature::VideoTracks));
    }

    virtual void ShutdownModule() override
    {
        // unregister player factory
        auto MediaModule = FModuleManager::GetModulePtr<IMediaModule>("Media");

        if (MediaModule != nullptr)
        {
            MediaModule->UnregisterPlayerFactory(*this);
        }
    }

    virtual void StartupModule() override
    {

        // supported file extensions
        SupportedFileExtensions.Add(TEXT("3gpp"));
        SupportedFileExtensions.Add(TEXT("aac"));
        SupportedFileExtensions.Add(TEXT("mp4"));
        SupportedFileExtensions.Add(TEXT("m3u8"));
        SupportedFileExtensions.Add(TEXT("webm"));

        // supported platforms
        SupportedPlatforms.Add(TEXT("Android"));

        // supported schemes
        SupportedUriSchemes.Add(TEXT("file"));
        /*        SupportedUriSchemes.Add(TEXT("http"));
                SupportedUriSchemes.Add(TEXT("httpd"));
                SupportedUriSchemes.Add(TEXT("https"));
                SupportedUriSchemes.Add(TEXT("mms"));
                SupportedUriSchemes.Add(TEXT("rtsp"));
                SupportedUriSchemes.Add(TEXT("rtspt"));
                SupportedUriSchemes.Add(TEXT("rtspu"));*/

        // register media player info
        auto MediaModule = FModuleManager::LoadModulePtr<IMediaModule>("Media");

        if (MediaModule != nullptr)
        {
            MediaModule->RegisterPlayerFactory(*this);
        }
    }

  private:
    /** List of supported media file types. */
    TArray<FString> SupportedFileExtensions;

    /** List of platforms that the media player support. */
    TArray<FString> SupportedPlatforms;

    /** List of supported URI schemes. */
    TArray<FString> SupportedUriSchemes;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAndroidVulkanVideoFactoryModule, AndroidVulkanVideoFactory);
