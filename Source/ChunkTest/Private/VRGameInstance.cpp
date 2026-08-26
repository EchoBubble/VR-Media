#include "VRGameInstance.h"

#include "Async/Async.h"
#include "ChunkDownloader.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "WebSocketsModule.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#endif

namespace VRCode
{
	constexpr int32 StartGame = 1001;
	constexpr int32 StartGameAck = 1002;
	constexpr int32 StopGame = 1003;
	constexpr int32 StopGameAck = 1004;
	constexpr int32 GameFinish = 1009;
	constexpr int32 GameFinishAck = 1010;
	constexpr int32 UploadProgress = 1082;
	constexpr int32 UploadProgressAck = 1083;
}


namespace VRChunkConfig
{
	const FString DeploymentName = TEXT("VRVideoLive");
	const FString PlatformName = TEXT("Android");
}

void UVRGameInstance::Init()
{
	Super::Init();

	bShuttingDown = false;
	bSuppressReconnect = false;
	bExitFlowStarted = false;

	const bool bConfigLoaded = LoadWebSocketConfig();//先读取ini作为兜底
	BindMediaEvents();

	if (!bConfigLoaded) return;

	if (bEnableRemoteConfig && !ConfigURL.IsEmpty())
	{
		DownloadRemoteConfig();
	}
	else
	{
		StartFromCurrentConfig();
	}
}

void UVRGameInstance::Shutdown()
{
	bShuttingDown = true;
	bSuppressReconnect = true;
	StopReconnectTimer();
	StopProgressTimer();
	UnbindMediaEvents();

	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	WS_Close();

	if (bChunkDownloaderInitialized)
	{
		FChunkDownloader::Shutdown();
		bChunkDownloaderInitialized = false;
	}

	Super::Shutdown();
}

void UVRGameInstance::WS_Connect(const FString& InServerURL)
{
	const FString URL = InServerURL.TrimStartAndEnd();
	if (URL.IsEmpty() || (!URL.StartsWith(TEXT("ws://")) && !URL.StartsWith(TEXT("wss://"))))
	{
		ReportError(TEXT("WebSocket地址无效。"));
		return;
	}

	if (bShuttingDown) return;

	LastWebSocketURL = URL;
	ReconnectAttemptCount = 0;
	bSuppressReconnect = false;
	bReconnectExhausted = false;
	ConnectWebSocketInternal(URL);
}

void UVRGameInstance::ConnectWebSocketInternal(const FString& URL)
{
	if (bShuttingDown || URL.IsEmpty()) return;

	StopReconnectTimer();

	// 每建立一条新连接就增加代数。旧连接迟到的回调会因为代数不匹配而被忽略。
	++WebSocketGeneration;
	const int32 ConnectionGeneration = WebSocketGeneration;

	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}

	FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets"));
	WebSocket = FWebSocketsModule::Get().CreateWebSocket(URL);

	TWeakObjectPtr<UVRGameInstance> WeakThis(this);

	WebSocket->OnConnected().AddLambda([WeakThis, ConnectionGeneration]()
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ConnectionGeneration]()
		{
			if (!WeakThis.IsValid()
				|| WeakThis->WebSocketGeneration != ConnectionGeneration
				|| WeakThis->bShuttingDown)
			{
				return;
			}

			UE_LOG(LogTemp, Warning, TEXT("[VRNET] Connected"));
			WeakThis->ReconnectAttemptCount = 0;
			WeakThis->bReconnectExhausted = false;
			if (!WeakThis->bStopping)
			{
				WeakThis->bSuppressReconnect = false;
			}
			WeakThis->StopReconnectTimer();
			WeakThis->OnWebSocketConnected.Broadcast();

			// 重连成功后立即上传一次最新状态；断线期间不缓存已经过时的进度包。
			if (!WeakThis->bDirectMode
				&& WeakThis->ActiveVideoId != INDEX_NONE
				&& WeakThis->ActivePlaylistIndex != INDEX_NONE
				&& WeakThis->MediaPlayer
				&& WeakThis->MediaPlayer->IsPlaying())
			{
				WeakThis->SendProgressInfo();
			}
		});
	});

	WebSocket->OnConnectionError().AddLambda([WeakThis, ConnectionGeneration](const FString& Error)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ConnectionGeneration, Error]()
		{
			if (!WeakThis.IsValid() || WeakThis->WebSocketGeneration != ConnectionGeneration) return;
			if (WeakThis->bSuppressReconnect || WeakThis->bShuttingDown) return;

			WeakThis->ReportError(FString::Printf(TEXT("WebSocket连接失败：%s"), *Error));
			WeakThis->ScheduleReconnect();
		});
	});

	WebSocket->OnClosed().AddLambda([WeakThis, ConnectionGeneration](int32 Code, const FString& Reason, bool bWasClean)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ConnectionGeneration, Code, Reason, bWasClean]()
		{
			if (!WeakThis.IsValid() || WeakThis->WebSocketGeneration != ConnectionGeneration) return;

			WeakThis->OnWebSocketClosed.Broadcast(Code, Reason, bWasClean);
			WeakThis->ScheduleReconnect();
		});
	});

	WebSocket->OnMessage().AddLambda([WeakThis, ConnectionGeneration](const FString& Message)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis, ConnectionGeneration, Message]()
		{
			if (!WeakThis.IsValid() || WeakThis->WebSocketGeneration != ConnectionGeneration) return;
			UE_LOG(LogTemp, Warning, TEXT("[VRNET] Received=%s"), *Message);
			WeakThis->OnWebSocketMessageReceived.Broadcast(Message);
			WeakThis->HandleProtocolMessage(Message);//处理协议消息
		});
	});

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRNET] Connecting: %s (retry %d)"),
		*URL,
		ReconnectAttemptCount
	);
	WebSocket->Connect();
}

void UVRGameInstance::ScheduleReconnect()
{
	if (bSuppressReconnect || bShuttingDown || LastWebSocketURL.IsEmpty() || WS_IsConnected()) return;

	if (UWorld* World = GetWorld())
	{
		if (World->GetTimerManager().IsTimerActive(ReconnectTimer)) return;
		const float RetryDelay = ReconnectAttemptCount < FastReconnectAttempts
			? ReconnectIntervalSeconds
			: PersistentReconnectIntervalSeconds;//前几次的检查间隔为5秒，后面所有的都为10秒

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[VRNET] 将在%.1f秒后进行第%d次重连。"),
			RetryDelay,
			ReconnectAttemptCount + 1
		);
		//非循环，而是每次尝试连接失败后，再次回到这个函数触发 timer 计时器
		World->GetTimerManager().SetTimer(
			ReconnectTimer,
			this,
			&UVRGameInstance::AttemptReconnect,
			RetryDelay,
			false
		);
	}
	else if (!bReconnectExhausted)
	{
		bReconnectExhausted = true;
		ReportError(TEXT("无法取得World，WebSocket不能安排自动重连。"));
	}
}

void UVRGameInstance::AttemptReconnect()
{
	StopReconnectTimer();//当前一次性 Timer 已经触发，清掉它的 Handle 和残留状态。

	if (bSuppressReconnect || bShuttingDown || WS_IsConnected() || LastWebSocketURL.IsEmpty()) return;

	++ReconnectAttemptCount;
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRNET] 开始第%d次重连。"),
		ReconnectAttemptCount
	);
	ConnectWebSocketInternal(LastWebSocketURL);
}

void UVRGameInstance::StopReconnectTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReconnectTimer);
	}
}

void UVRGameInstance::WS_SendMessage(const FString& Message)
{
	if (!WS_IsConnected())
	{
		ReportError(TEXT("WebSocket未连接。"));
		return;
	}

	WebSocket->Send(Message);
}

void UVRGameInstance::WS_Close()
{
	bSuppressReconnect = true;
	StopReconnectTimer();

	if (WebSocket.IsValid())
	{
		WebSocket->Close();
		WebSocket.Reset();
	}
}

bool UVRGameInstance::WS_IsConnected() const
{
	return WebSocket.IsValid() && WebSocket->IsConnected();
}

bool UVRGameInstance::ConnectUsingConfig()
{
	
	const FString URL = GetConfiguredWebSocketURL();
	if (URL.IsEmpty())
	{
		ReportError(TEXT("DefaultGame.ini中的ServerURL为空。"));
		return false;
	}

	WS_Connect(URL);
	return true;
}

void UVRGameInstance::ReloadVRConfiguration()
{
	LoadWebSocketConfig();
}

bool UVRGameInstance::LoadWebSocketConfig()
{
	if (!GConfig)
	{
		ReportError(TEXT("无法读取配置：GConfig为空。"));
		return false;
	}

	const TCHAR* WebSocketSection = TEXT("VRWebSocket");

	// 先把 INI 当作兜底值读入内存。远程 JSON 或缓存成功后，只覆盖其中存在的字段。
	ServerURL.Reset();
	DeviceIdOverride.Reset();
	PakBuildUrl.Reset();
	ChunkIds.Reset();
	PlaylistDurationsSeconds.Reset();
	
	GConfig->GetBool(
		WebSocketSection,
		TEXT("bAutoConnectOnInit"),
		bAutoConnectOnInit,
		GGameIni
	);

	const bool bHasServerURL = GConfig->GetString(
		WebSocketSection,
		TEXT("ServerURL"),
		ServerURL,
		GGameIni
	);

	GConfig->GetString(
		WebSocketSection,
		TEXT("DeviceIdOverride"),
		DeviceIdOverride,
		GGameIni
	);

	GConfig->GetInt(WebSocketSection, TEXT("ClientType"), ClientType, GGameIni);
	GConfig->GetBool(WebSocketSection, TEXT("bUseSignedHandshake"), bUseSignedHandshake, GGameIni);
	GConfig->GetString(WebSocketSection, TEXT("SignaturePrefix"), SignaturePrefix, GGameIni);
	GConfig->GetString(WebSocketSection, TEXT("SignatureSuffix"), SignatureSuffix, GGameIni);
	GConfig->GetFloat(WebSocketSection, TEXT("UploadIntervalSeconds"), UploadIntervalSeconds, GGameIni);
	GConfig->GetFloat(WebSocketSection, TEXT("QuitDelaySeconds"), QuitDelaySeconds, GGameIni);

	const TCHAR* RemoteSection = TEXT("VRRemoteConfig");
	GConfig->GetBool(RemoteSection, TEXT("bEnableRemoteConfig"), bEnableRemoteConfig, GGameIni);
	GConfig->GetString(RemoteSection, TEXT("ConfigURL"), ConfigURL, GGameIni);
	GConfig->GetFloat(RemoteSection, TEXT("RequestTimeoutSeconds"), RequestTimeoutSeconds, GGameIni);

	const TCHAR* ChunkSection = TEXT("VRChunk");
	GConfig->GetString(ChunkSection, TEXT("PakBuildUrl"), PakBuildUrl, GGameIni);
	GConfig->GetInt(
		ChunkSection,
		TEXT("TargetDownloadsInFlight"),
		ChunkTargetDownloadsInFlight,
		GGameIni
	);
	GConfig->GetInt(
		ChunkSection,
		TEXT("DownloadPriority"),
		ChunkDownloadPriority,
		GGameIni
	);

	//读取出来的都是字符串
	TArray<FString> ChunkIdStrings;//由于配置文件中这里是一个数组，又为了获取不同的值，所以用数组接收
	GConfig->GetArray(ChunkSection, TEXT("ChunkIds"), ChunkIdStrings, GGameIni);

	//转化为整型；TSet 是一个集合，用于记录哪些 ID 已经添加过了，避免重复添加
	TSet<int32> SeenChunkIds;
	for (const FString& ChunkIdText : ChunkIdStrings)
	{
		//Atoi 源于 ASCII to Integer，意思是把字符串转换成整数
		const int32 ChunkId = FCString::Atoi(*ChunkIdText.TrimStartAndEnd());//这里的 * 不是解引用，而是 FString 的操作，取得内部 TCHAR* 字符串地址
		if (ChunkId > 0 && !SeenChunkIds.Contains(ChunkId))
		{
			SeenChunkIds.Add(ChunkId);
			ChunkIds.Add(ChunkId);
		}
	}

	const TCHAR* MediaSection = TEXT("VRMedia");

	TArray<FString> DurationStrings;
	GConfig->GetArray(
		MediaSection,
		TEXT("VideoDurations"),
		DurationStrings,
		GGameIni
	);

	bool bDurationsValid = true;

	for (const FString& DurationText : DurationStrings)
	{
		const float Seconds =
			FCString::Atof(*DurationText.TrimStartAndEnd());//字符串 → float

		if (Seconds <= 0.0f)
		{
			bDurationsValid = false;
			break;
		}

		PlaylistDurationsSeconds.Add(Seconds);
	}

	if (!bDurationsValid)
	{
		PlaylistDurationsSeconds.Reset();

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[VRPROGRESS] DefaultGame.ini中的VideoDurations存在无效值。")
		);
	}
	
	ServerURL = ServerURL.TrimStartAndEnd();
	DeviceIdOverride = DeviceIdOverride.TrimStartAndEnd();
	ConfigURL = ConfigURL.TrimStartAndEnd();
	PakBuildUrl = PakBuildUrl.TrimStartAndEnd();
	UploadIntervalSeconds = FMath::Max(UploadIntervalSeconds, 0.1f);
	QuitDelaySeconds = FMath::Max(QuitDelaySeconds, 0.0f);
	RequestTimeoutSeconds = FMath::Max(RequestTimeoutSeconds, 1.0f);
	ChunkTargetDownloadsInFlight = FMath::Max(ChunkTargetDownloadsInFlight, 1);//并发下载数量最少必须为 1

	UE_LOG(LogTemp, Warning, TEXT("WebSocket地址：%s"), *ServerURL);
	UE_LOG(LogTemp, Warning, TEXT("远程配置地址：%s"), *ConfigURL);
	UE_LOG(LogTemp, Warning, TEXT("Pak构建地址：%s"), *PakBuildUrl);
	UE_LOG(LogTemp, Warning, TEXT("视频Chunk数量：%d"), ChunkIds.Num());

	const bool bHasRemote = bEnableRemoteConfig && !ConfigURL.IsEmpty();
	if ((!bHasServerURL || ServerURL.IsEmpty()) && !bHasRemote)
	{
		ReportError(TEXT("ServerURL和ConfigURL都为空。"));
		return false;
	}

	return true;
}

FString UVRGameInstance::GetRemoteConfigCachePath() const//拼接文件地址，用于保存远程配置缓存文件的地址
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/vr_config.json"));//获取项目可写入的 Saved 目录
}

void UVRGameInstance::DownloadRemoteConfig()
{
	const FString URL = ConfigURL.TrimStartAndEnd();//在Init时通过LoadWebSocketConfig()获取远程地址
	if (!URL.StartsWith(TEXT("http://")) && !URL.StartsWith(TEXT("https://")))
	{
		UE_LOG(LogTemp, Warning, TEXT("ConfigURL无效，使用DefaultGame.ini。"));
		StartFromCurrentConfig();//地址无效时直接执行
		return;
	}

	//HTTP请求是异步的，为了防止对象销毁，所以这里需要用弱指针；发送请求后等几秒，服务器返回结果，才执行回调
	TWeakObjectPtr<UVRGameInstance> WeakThis(this);
	//定义一个名叫 UseCache 的本地小函数，这个小函数能够使用 WeakThis，没有参数
	//尝试使用缓存，如果没有，就使用 ini 配置文件兜底
	const auto UseCache = [WeakThis]()
	{
		if (!WeakThis.IsValid() || WeakThis->bStartupApplied) return;//不存在或者程序已经启动过就返回

		FString Text;
		if (FFileHelper::LoadFileToString(Text, *WeakThis->GetRemoteConfigCachePath())
			&& WeakThis->ApplyRemoteConfigText(Text, false))//将地址里的字符读取出来并返回给Text，并且缓存中的JSON也有效
		{
			UE_LOG(LogTemp, Warning, TEXT("使用本地远程配置缓存。"));
			return;
		}

		UE_LOG(LogTemp, Warning, TEXT("使用DefaultGame.ini兜底。"));
		WeakThis->StartFromCurrentConfig();
	};

	//头显在浏览器中访问 JSON 文件，这里相当于一次 HTTP 的请求
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(URL);//告诉访问的地址
	Request->SetVerb(TEXT("GET"));//获取内容
	Request->SetTimeout(RequestTimeoutSeconds);//最多等待多少秒
	//HTTP 请求不会立刻得到结果，所以要绑定回调
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, UseCache](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)//参数一用不上，response为结果，参数三为请求是否正常完成
		{
			//这里主要是防止重复或者延迟执行，例如 HTTP 请求发出，因为某种原因先执行了缓存兜底，缓存启用成功后，但是原来的 HTTP 回调晚了一会来了，造成重复
			if (!WeakThis.IsValid() || WeakThis->bStartupApplied) return;

			const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
			//bSuccess不一定代表成功给，因为可能返回404文件不存在或者500服务器错误，所以还要检查状态码，200成功
			if (bSuccess && Code >= 200 && Code < 300
				&& WeakThis->ApplyRemoteConfigText(Response->GetContentAsString(), true))//下载配置并保存到 saved中
			{
				UE_LOG(LogTemp, Warning, TEXT("使用远程配置。"));
				return;//这里的return返回的是 HTTP 回调 Lambda ,不是最外面的函数
			}

			UE_LOG(LogTemp, Warning, TEXT("远程配置请求失败，HTTP=%d。"), Code);
			UseCache();//如果远端关闭或者错误等，尝试使用缓存的文件
		}
	);

	UE_LOG(LogTemp, Warning, TEXT("下载远程配置：%s"), *URL);
	if (!Request->ProcessRequest()) UseCache();//发送请求，失败了就用小函数执行
}

bool UVRGameInstance::ApplyRemoteConfigText(const FString& Text, bool bSaveCache)
{
	if (bStartupApplied) return true;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	double VersionValue = 0.0;
	FString Mode;
	if (!Root->TryGetNumberField(TEXT("version"), VersionValue)
		|| !Root->TryGetStringField(TEXT("mode"), Mode))
	{
		return false;
	}

	const int32 Version = FMath::RoundToInt(VersionValue);
	if (Version != 1 && Version != 2)//目前版本只支持到2，更高版本需要再添加
	{
		return false;
	}

	// 所有新值先放在局部变量里；整份 JSON 验证通过后再一次性提交，避免半覆盖。
	FString NewServerURL = ServerURL;
	FString NewPakBuildUrl = PakBuildUrl;
	TArray<int32> NewChunkIds = ChunkIds;
	TArray<float> NewPlaylistDurations = PlaylistDurationsSeconds;
	bool bNewDirectMode = bDirectMode;
	int32 NewDirectVideoId = DirectVideoId;
	int32 NewDirectLevelId = DirectLevelId;
	float NewDirectProgress = DirectProgress;

	Mode = Mode.TrimStartAndEnd().ToLower();

	if (Mode == TEXT("websocket"))
	{
		FString URL;
		if (!Root->TryGetStringField(TEXT("wsUrl"), URL))
		{
			return false;
		}

		URL = URL.TrimStartAndEnd();
		if (!URL.StartsWith(TEXT("ws://")) && !URL.StartsWith(TEXT("wss://")))
		{
			return false;
		}

		bNewDirectMode = false;
		NewServerURL = URL;
	}
	else if (Mode == TEXT("direct"))
	{
		double VideoId = 0.0;
		double LevelId = 0.0;
		double Progress = 0.0;
		if (!Root->TryGetNumberField(TEXT("videoId"), VideoId)
			|| !Root->TryGetNumberField(TEXT("levelId"), LevelId)
			|| !Root->TryGetNumberField(TEXT("progress"), Progress))
		{
			return false;
		}

		NewDirectVideoId = FMath::RoundToInt(VideoId);
		NewDirectLevelId = FMath::RoundToInt(LevelId);
		NewDirectProgress = FMath::Clamp(static_cast<float>(Progress), 0.0f, 1.0f);

		// videoId是服务器业务ID；levelId字段表示Playlist索引（协议cl）。
		if (NewDirectVideoId < 0 || NewDirectLevelId < 0 || !Playlist
			|| NewDirectLevelId >= Playlist->Num())
		{
			return false;
		}

		bNewDirectMode = true;
	}
	else
	{
		return false;
	}

	FString RemotePakBuildUrl;//Chunk 包网络地址
	if (Root->TryGetStringField(TEXT("pakBuildUrl"), RemotePakBuildUrl))
	{
		RemotePakBuildUrl = RemotePakBuildUrl.TrimStartAndEnd();

		FString TestBaseUrl;//获取测试地址
		FString TestBuildId;//获取测试 Build 名称
		if (!ParsePakBuildUrl(RemotePakBuildUrl, TestBaseUrl, TestBuildId))
		{
			return false;
		}

		NewPakBuildUrl = RemotePakBuildUrl;
	}

	const TArray<TSharedPtr<FJsonValue>>* RemoteChunkValues = nullptr;
	if (Root->TryGetArrayField(TEXT("chunkIds"), RemoteChunkValues))
	{
		if (!RemoteChunkValues || RemoteChunkValues->IsEmpty())
		{
			return false;
		}

		TArray<int32> ParsedChunkIds;//保存最后解析成功的 ID
		TSet<int32> SeenChunkIds;//记录哪些 ID 已经出现，防止重复

		for (const TSharedPtr<FJsonValue>& Value : *RemoteChunkValues)
		{
			double ChunkNumber = 0.0;
			if (!Value.IsValid() || !Value->TryGetNumber(ChunkNumber))
			{
				return false;//无效或者不是数字，就返回
			}

			const int32 ChunkId = FMath::RoundToInt(ChunkNumber);//将 double 四舍五入为整数
			if (ChunkId <= 0 || FMath::Abs(ChunkNumber - static_cast<double>(ChunkId)) > 0.000001)//这里是检查原来的数字和取整后的数字相差是不是太多
			{
				return false;
			}

			if (!SeenChunkIds.Contains(ChunkId))//防止重复添加
			{
				SeenChunkIds.Add(ChunkId);
				ParsedChunkIds.Add(ChunkId);
			}
		}
		//将结果直接移交给 NewChunkIds，避免再复制一遍，也就是 A 直接搬家到 B
		NewChunkIds = MoveTemp(ParsedChunkIds);
	}

	const TArray<TSharedPtr<FJsonValue>>* DurationValues = nullptr;
	if (Root->TryGetArrayField(TEXT("videoDurations"), DurationValues))
	{
		if (!DurationValues || !Playlist || DurationValues->Num() != Playlist->Num()) return false;

		NewPlaylistDurations.Reset(DurationValues->Num());
		for (const TSharedPtr<FJsonValue>& Value : *DurationValues)
		{
			double Seconds = 0.0;
			if (!Value.IsValid() || !Value->TryGetNumber(Seconds) || Seconds <= 0.0) return false;
			NewPlaylistDurations.Add(static_cast<float>(Seconds));
		}
	}

	// 最终值可以来自远程 JSON，也可以继续沿用程序启动时读取的 INI。
	FString FinalBaseUrl;//获取最终地址
	FString FinalBuildId;//获取最终 Build 名称
	if (!ParsePakBuildUrl(NewPakBuildUrl, FinalBaseUrl, FinalBuildId) || NewChunkIds.IsEmpty())
	{
		return false;//虽然多余，但这里是防止远程文件没有存放地址
	}

	ServerURL = MoveTemp(NewServerURL);
	PakBuildUrl = MoveTemp(NewPakBuildUrl);
	ChunkIds = MoveTemp(NewChunkIds);
	PlaylistDurationsSeconds = MoveTemp(NewPlaylistDurations);
	bDirectMode = bNewDirectMode;
	DirectVideoId = NewDirectVideoId;
	DirectLevelId = NewDirectLevelId;
	DirectProgress = NewDirectProgress;

	if (bSaveCache)
	{
		const FString Path = GetRemoteConfigCachePath();//获取可缓存的地址
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(
			Text,
			*Path,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM
		);
	}

	StartFromCurrentConfig();
	return true;
}

void UVRGameInstance::StartFromCurrentConfig()
{
	if (bStartupApplied) return;
	bStartupApplied = true;
	if (Playlist && PlaylistDurationsSeconds.Num() != Playlist->Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRPROGRESS] videoDurations数量与Playlist不一致，gp暂按视频数量等权计算。"));
	}

	// 两种模式都先自主准备所有视频 Pak。
	StartChunkPreparation();

	if (bDirectMode)
	{
		UE_LOG(LogTemp, Warning, TEXT("Direct模式：vid=%d cl=%d clp=%.3f"),
			DirectVideoId, DirectLevelId, DirectProgress);

		// StartGame 会保存 Pending 状态。Chunk 尚未完成时先等待，挂载完成后自动继续。
		if (!StartGame(DirectVideoId, DirectLevelId, DirectProgress))
		{
			ReportError(TEXT("Direct模式启动失败。"));
		}
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("WebSocket模式。视频Pak在后台准备，中控连接并行进行。"));
	if (bAutoConnectOnInit)
	{
		ConnectUsingConfig();
	}
}


bool UVRGameInstance::ParsePakBuildUrl(const FString& InUrl, FString& OutCdnBaseUrl, FString& OutContentBuildId) const
{
	OutCdnBaseUrl.Reset();//清空输出变量，避免保存旧的内容
	OutContentBuildId.Reset();

	FString Url = InUrl.TrimStartAndEnd();
	while (Url.EndsWith(TEXT("/")))//只要地址最后还是 /，就删除最后一个字符。
	{
		Url.LeftChopInline(1, false);//从字符串右边删除 1 个字符
	}

	if (!Url.StartsWith(TEXT("http://")) && !Url.StartsWith(TEXT("https://")))
	{
		return false;
	}

	int32 LastSlashIndex = INDEX_NONE;
	if (!Url.FindLastChar(TEXT('/'), LastSlashIndex))//保存这个 / 在字符串中的位置。这里是网络地址和Build中间的 /
	{
		return false;
	}

	const int32 SchemeEnd = Url.Find(TEXT("://"));//确保地址存在合法协议部分，http://
	if (SchemeEnd == INDEX_NONE || LastSlashIndex <= SchemeEnd + 2 || LastSlashIndex >= Url.Len() - 1)
	{
		//没找到 ://，失败。最后的 || / 位置不正常，失败。|| / 后面没有 Build 名称，失败。
		return false;
	}

	OutCdnBaseUrl = Url.Left(LastSlashIndex);//启用最后 / 左边的内容，这里是服务器地址
	OutContentBuildId = Url.Mid(LastSlashIndex + 1);//取得最后一个 / 右边的内容

	return !OutCdnBaseUrl.IsEmpty()
		&& !OutContentBuildId.IsEmpty()
		&& !OutContentBuildId.Contains(TEXT("?"))
		&& !OutContentBuildId.Contains(TEXT("#"));
}

bool UVRGameInstance::ApplyChunkBaseUrlToRuntimeConfig(const FString& CdnBaseUrl) const
{
	if (!GConfig || CdnBaseUrl.IsEmpty())
	{
		return false;
	}

	// ChunkDownloader 会按 DeploymentName 读取 CdnBaseUrls。
	// 这里覆盖 GGameIni 的内存配置，所以远程 JSON 改地址后不需要重新打 APK。
	const FString Section = FString::Printf(
		TEXT("/Script/Plugins.ChunkDownloader %s"),
		*VRChunkConfig::DeploymentName
	);//告诉配置文件要修改配置文件中的哪一块

	TArray<FString> CdnBaseUrls;
	CdnBaseUrls.Add(CdnBaseUrl);
	GConfig->SetArray(*Section, TEXT("CdnBaseUrls"), CdnBaseUrls, GGameIni);//用最终地址覆盖当前运行时的下载地址数组
	return true;
}

void UVRGameInstance::StartChunkPreparation()//根据最终确定的配置，正式启动视频 Pak 的准备流程
{
	if (ChunkPreparationState != EVRChunkPreparationState::NotStarted)//防止重复启动
	{
		return;
	}

#if WITH_EDITOR
	// PIE/编辑器运行时直接使用工程 Content/Movies 下的本地视频，不需要下载 Android Pak。
	ChunkPreparationState = EVRChunkPreparationState::Ready;
	UE_LOG(LogTemp, Warning, TEXT("[VRCHUNK] 编辑器运行：跳过ChunkDownloader，直接使用本地Playlist。"));
	return;
#endif

	//拆分并确认 PakBuildUrl  再整理一次 ChunkIds
	//网络模式下这里并非多余，因为 ChunkDownloader 缓存地址和远程配置地址不是一个东西，需要告诉 CD 最新地址
	FString CdnBaseUrl;
	FString ContentBuildId;
	if (!ParsePakBuildUrl(PakBuildUrl, CdnBaseUrl, ContentBuildId))
	{
		MarkChunkPreparationFailed(FString::Printf(
			TEXT("PakBuildUrl无效：%s。正确格式示例：http://服务器:端口/VRVideoBuild01"),
			*PakBuildUrl
		));
		return;
	}

	TArray<int32> ValidChunkIds;
	TSet<int32> SeenChunkIds;
	for (const int32 ChunkId : ChunkIds)
	{
		if (ChunkId > 0 && !SeenChunkIds.Contains(ChunkId))
		{
			SeenChunkIds.Add(ChunkId);
			ValidChunkIds.Add(ChunkId);
		}
	}
	ChunkIds = MoveTemp(ValidChunkIds);

	if (ChunkIds.IsEmpty())
	{
		MarkChunkPreparationFailed(TEXT("没有配置任何视频ChunkIds。"));
		return;
	}
	//把最终服务器地址临时写进 UE 的运行时配置，让 ChunkDownloader 知道去哪里下载。
	if (!ApplyChunkBaseUrlToRuntimeConfig(CdnBaseUrl))
	{
		MarkChunkPreparationFailed(TEXT("无法写入ChunkDownloader运行时CDN地址。"));
		return;
	}

	ChunkPreparationState = EVRChunkPreparationState::UpdatingManifest;

	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetOrCreate();
	Downloader->Initialize(
		VRChunkConfig::PlatformName,
		FMath::Max(ChunkTargetDownloadsInFlight, 1)
	);//初始化一次性最多下载数量
	bChunkDownloaderInitialized = true;

	//ChunkDownloader 是否成功读取了头显本地以前保存的 Build/Manifest 清单信息,第一次运行为 false
	const bool bCachedBuildLoaded = Downloader->LoadCachedBuild(VRChunkConfig::DeploymentName);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] Update Manifest: %s/%s/BuildManifest-Android.txt，CachedBuild=%s"),
		*CdnBaseUrl,
		*ContentBuildId,
		bCachedBuildLoaded ? TEXT("true") : TEXT("false")
	);

	//根据前面设置的 CDN 地址和 Build 名称，去服务器获取当前 Manifest，对照文件是 BuildManifest-Android.txt
	TWeakObjectPtr<UVRGameInstance> WeakThis(this);
	Downloader->UpdateBuild(
		VRChunkConfig::DeploymentName,
		ContentBuildId,
		[WeakThis, bCachedBuildLoaded](bool bSuccess)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, bCachedBuildLoaded]()
			{
				if (!WeakThis.IsValid() || WeakThis->bShuttingDown)
				{
					return;
				}

				WeakThis->HandleChunkManifestUpdated(bSuccess, bCachedBuildLoaded);
			});
		}
	);
}

void UVRGameInstance::HandleChunkManifestUpdated(bool bSuccess, bool bCachedBuildLoaded)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] Manifest update: %s"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")
	);

	if (bSuccess)//使用服务器最新 Manifest
	{
		if (!ValidateConfiguredChunksAgainstManifest())//检查 ChunkIds 是否存在
		{
			MarkChunkPreparationFailed(TEXT("Manifest中缺少DefaultGame.ini/JSON配置的某个Chunk ID。"));
			return;
		}
		//开始下载分区,但插件内部会判断 Chunk 当前状态，第二次运行，上一次的 pak 挂载状态会消失，但会重新读取缓存，不会重复下载
		StartChunkDownload();
		return;
	}

	// Pak服务器暂时不可用时，只允许使用已经完整缓存到头显上的全部Chunk。
	// 这样离线时不会因为缺失文件进入无限重试下载。
	if (bCachedBuildLoaded && AreAllConfiguredChunksCachedOrMounted())//含有旧的清单且所有Pak都完整下载过
	{
		UE_LOG(LogTemp, Warning, TEXT("[VRCHUNK] 使用本地缓存Manifest和已缓存Pak。"));
		StartChunkMount();//挂在旧的 Pak，继续播放
		return;
	}

	MarkChunkPreparationFailed(TEXT("无法更新Manifest，并且本地缓存不足以播放全部视频。"));
}

bool UVRGameInstance::ValidateConfiguredChunksAgainstManifest() const
{
	const TSharedPtr<FChunkDownloader> Downloader = FChunkDownloader::Get();
	if (!Downloader.IsValid())
	{
		return false;
	}

	TArray<int32> ManifestChunkIds;//从当前 Manifest 中取出所有 Chunk ID,例如 1001、1002
	Downloader->GetAllChunkIds(ManifestChunkIds);

	for (const int32 ChunkId : ChunkIds)//查看清单是否包含 1001、1002 的 ID
	{
		if (!ManifestChunkIds.Contains(ChunkId))
		{
			UE_LOG(LogTemp, Error, TEXT("[VRCHUNK] Manifest missing Chunk %d"), ChunkId);
			return false;
		}
	}

	return true;
}

void UVRGameInstance::StartChunkDownload()
{
	if (ChunkPreparationState == EVRChunkPreparationState::Failed || bShuttingDown)
	{
		return;//已经失败或者程序正在关闭，就不再下载。
	}

	ChunkPreparationState = EVRChunkPreparationState::Downloading;//表示当前流程进入下载阶段。
	TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();//获取 Downloader

	for (const int32 ChunkId : ChunkIds)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("[VRCHUNK] Chunk %d status before download: %d"),
			ChunkId,
			static_cast<int32>(Downloader->GetChunkStatus(ChunkId))
		);
	}

	TWeakObjectPtr<UVRGameInstance> WeakThis(this);
	Downloader->DownloadChunks(
		ChunkIds,
		[WeakThis](bool bSuccess)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()//把整个 ChunkIds 数组交给插件下载 然后切回游戏线程执行
			{
				if (!WeakThis.IsValid() || WeakThis->bShuttingDown)
				{
					return;
				}

				WeakThis->HandleChunkDownloadFinished(bSuccess);
			});
		},
		ChunkDownloadPriority
	);
	Downloader->BeginLoadingMode(
	[](bool bSuccess)
	{
		UE_LOG(LogTemp, Warning,TEXT("[VRCHUNK] LoadingMode finished: %s"),
			bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
	});
}

void UVRGameInstance::HandleChunkDownloadFinished(bool bSuccess)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] Download all chunks: %s"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")
	);

	if (!bSuccess)
	{
		MarkChunkPreparationFailed(TEXT("视频Pak下载失败。"));
		return;
	}

	StartChunkMount();
}

void UVRGameInstance::StartChunkMount()//开始挂载
{
	if (ChunkPreparationState == EVRChunkPreparationState::Failed || bShuttingDown)
	{
		return;
	}

	const TSharedRef<FChunkDownloader> Downloader = FChunkDownloader::GetChecked();

	bool bAlreadyMounted = true;
	for (const int32 ChunkId : ChunkIds)//判断所有 Chunk 是否已经挂载
	{
		if (Downloader->GetChunkStatus(ChunkId) != FChunkDownloader::EChunkStatus::Mounted)
		{
			bAlreadyMounted = false;//发现任何一个 Chunk 没挂载：bAlreadyMounted = false，break 结束检查
			break;//退出这个 for 循环，不是退出整个函数
		}
	}

	if (bAlreadyMounted)//如果所有 Chunk 已经挂载
	{
		HandleChunkMountFinished(true);//不需要重复挂载，直接按“挂载成功”处理。
		return;
	}
	//存在未挂载的 Chunk
	ChunkPreparationState = EVRChunkPreparationState::Mounting;

	TWeakObjectPtr<UVRGameInstance> WeakThis(this);
	//此时才真正调用插件挂载。
	Downloader->MountChunks(
		ChunkIds,
		[WeakThis](bool bSuccess)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess]()
			{
				if (!WeakThis.IsValid() || WeakThis->bShuttingDown)
				{
					return;
				}

				WeakThis->HandleChunkMountFinished(bSuccess);
			});
		}
	);
}

void UVRGameInstance::HandleChunkMountFinished(bool bSuccess)
{
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] Mount all chunks: %s"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAILED")
	);

	if (!bSuccess)
	{
		MarkChunkPreparationFailed(TEXT("视频Pak挂载失败。"));
		return;
	}

	ChunkPreparationState = EVRChunkPreparationState::Ready;
	ContinuePendingPlaybackAfterChunksReady();//如果之前有视频在等待，就继续播放
}

bool UVRGameInstance::AreAllConfiguredChunksCachedOrMounted() const
{
	const TSharedPtr<FChunkDownloader> Downloader = FChunkDownloader::Get();
	if (!Downloader.IsValid() || ChunkIds.IsEmpty())
	{
		return false;
	}

	for (const int32 ChunkId : ChunkIds)
	{
		const FChunkDownloader::EChunkStatus Status = Downloader->GetChunkStatus(ChunkId);
		if (Status != FChunkDownloader::EChunkStatus::Cached
			&& Status != FChunkDownloader::EChunkStatus::Mounted)
		{
			return false;
		}
	}

	return true;
}

void UVRGameInstance::ContinuePendingPlaybackAfterChunksReady()
{
	if (ChunkPreparationState != EVRChunkPreparationState::Ready
		|| !bPendingStart
		|| bStopping
		|| bShuttingDown)//主要是 bPendingStart，如果在挂载前就准备播放，那么这里就是补一下播放
	{
		return;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] 所有视频Pak已就绪，继续打开Playlist[%d]。"),
		PendingPlaylistIndex
	);

	if (!OpenPendingVideo())
	{
		bPendingStart = false;
	}
}

void UVRGameInstance::MarkChunkPreparationFailed(const FString& Reason)
{
	ChunkPreparationState = EVRChunkPreparationState::Failed;
	bPendingStart = false;
	bWaitingStartAck = false;
	ReportError(FString::Printf(TEXT("[VRCHUNK] %s"), *Reason));
}

FString UVRGameInstance::GetConfiguredWebSocketURL() const
{
	const FString BaseURL = ServerURL.TrimStartAndEnd();
	if (BaseURL.IsEmpty() || !bUseSignedHandshake)
	{
		return BaseURL;
	}

	const FString DeviceId = GetResolvedDeviceId();//获取设备 ID
	UE_LOG(LogTemp,Warning,TEXT("[VRNET] ResolvedDeviceId=%s"),*DeviceId);
	const FString Time = FString::FromInt(FDateTime::Now().GetYear());
	const FString SignText = FString::Printf(
		TEXT("%s_%s_%d_%s_%s"),
		*SignaturePrefix,
		*DeviceId,
		ClientType,
		*Time,
		*SignatureSuffix
	);

	const FString Sign = FMD5::HashAnsiString(*SignText);
	const FString Join = BaseURL.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");

	return FString::Printf(
		TEXT("%s%sid=%s&type=%d&t=%s&s=%s"),
		*BaseURL,
		*Join,
		*DeviceId,
		ClientType,
		*Time,
		*Sign
	);
}

FString UVRGameInstance::GetPlatformDeviceId() const
{
	#if PLATFORM_ANDROID

		if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
		{
			jmethodID MethodId = FJavaWrapper::FindMethod(
				Env,
				FJavaWrapper::GameActivityClassID,
				"AndroidThunkJava_GetAndroidDeviceName",
				"()Ljava/lang/String;",
				true
			);

			if (MethodId)
			{
				jstring JavaDeviceName = static_cast<jstring>(
					FJavaWrapper::CallObjectMethod(
						Env,
						FJavaWrapper::GameActivityThis,
						MethodId
					)
				);

				if (JavaDeviceName)
				{
					FString DeviceName =
						FJavaHelper::FStringFromLocalRef(
							Env,
							JavaDeviceName
						).TrimStartAndEnd();

					if (!DeviceName.IsEmpty())
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT("[VRNET] AndroidDeviceName=%s"),
							*DeviceName
						);

						return DeviceName;
					}
				}
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("[VRNET] Failed to read Android device_name, using fallback.")
			);
		}

	#endif
	FString Id = FPlatformMisc::GetDeviceId().TrimStartAndEnd();

	// PC端某些环境可能拿不到GetDeviceId，使用UE为当前电脑用户生成的MachineId兜底。
	if (Id.IsEmpty())
	{
		Id = FPlatformMisc::GetMachineId().ToString(EGuidFormats::Digits);
	}
	UE_LOG(LogTemp,Warning,TEXT("[VRNET] FallbackDeviceId=%s"),*Id);
	return Id.IsEmpty() ? TEXT("UnknownDevice") : Id;
}

FString UVRGameInstance::GetAndroidADeviceID() const
{
	return GetPlatformDeviceId();
}

FString UVRGameInstance::GetResolvedDeviceId() const
{
	const FString OverrideId = DeviceIdOverride.TrimStartAndEnd();
	return OverrideId.IsEmpty() ? GetPlatformDeviceId() : OverrideId;
}

void UVRGameInstance::HandleProtocolMessage(const FString& Message)
{
	//这里必须用 JSON 解析，不然就只是一堆字符串，无法拆解
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())//解析 JSON
	{
		ReportError(TEXT("收到的JSON无效。"));
		return;
	}

	double CodeValue = 0.0;//JSON 层读到的通用类型是 double
	if (!Root->TryGetNumberField(TEXT("code"), CodeValue))//读取 code 判断服务器要做什么
	{
		ReportError(TEXT("JSON缺少code。"));
		return;
	}

	const int32 Code = FMath::RoundToInt(CodeValue);//转化为整型

	if (Code == VRCode::StartGame)
	{
		//content 是子 JSON 对象，在这里的数据中是带有一个组 {}，所以得先获取这个子 FJsonObject
		//只想智能指针的指针，这里就是看 Content 指向的那个 TSharedPtr<FJsonObject>，里面是否真的管理着一个有效的 FJsonObject。
		const TSharedPtr<FJsonObject>* Content = nullptr;
		if (!Root->TryGetObjectField(TEXT("content"), Content) || !Content || !Content->IsValid())
		{
			ReportError(TEXT("1001缺少content。"));
			return;
		}

		//要求 JSON 中必须有 vid、cl、clp 并获取它
		double Vid = 0.0;
		double Cl = 0.0;
		double Clp = 0.0;
		if (!(*Content)->TryGetNumberField(TEXT("vid"), Vid)
			|| !(*Content)->TryGetNumberField(TEXT("cl"), Cl)
			|| !(*Content)->TryGetNumberField(TEXT("clp"), Clp))//取出指向它指向的智能指针
		{
			ReportError(TEXT("1001缺少vid、cl或clp。"));
			return;
		}

		// 保存服务端发来的原始进度，方便通过日志确认中控到底发的是多少。
		const float RawClp = static_cast<float>(Clp);
		// 客户端内部的进度统一使用 0～1。
		//
		// 兼容两种服务端格式：
		// 1. 服务端发送 0～1，例如 0.7 表示 70%。
		// 2. 服务端发送 0～100，例如 70 表示 70%。
		//
		// 当收到的值大于 1 时，认为它是百分数，因此除以 100。
		// 这样本地测试发送 0.7 仍然正常，正式中控发送 70 也能正常。
		const float NormalizedClp = RawClp > 1.0f ? RawClp / 100.0f : RawClp;

		// 最后再限制到合法的 0～1，防止服务端发送负数或大于 100 的异常值。
		const float SafeClp = FMath::Clamp(NormalizedClp, 0.0f, 1.0f);
		//真正的游戏启动
		StartGame(FMath::RoundToInt(Vid), FMath::RoundToInt(Cl), SafeClp);
	}
	else if (Code == VRCode::StopGame)
	{
		StopGame(true);
	}
	else if (Code == VRCode::GameFinishAck)
	{
		// 1010 仍然记录给蓝图，但不再决定客户端什么时候退出。
		// 客户端在发送 1009 后会按照本地计时器自行返回 Beacon 并退出。
		OnVRProtocolAckReceived.Broadcast(Code);
		bWaitingFinishAck = false;
		UE_LOG(LogTemp, Log, TEXT("[VREXIT] Received 1010. Local exit flow continues independently."));
	}
	else if (Code == VRCode::UploadProgressAck
		|| Code == VRCode::StartGameAck
		|| Code == VRCode::StopGameAck)
	{
		OnVRProtocolAckReceived.Broadcast(Code);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Unknown protocol code: %d"), Code);
	}
}

void UVRGameInstance::StartGameFromMessage(const FString& Message)
{
	HandleProtocolMessage(Message);
}

bool UVRGameInstance::SendInfo(int32 Code, bool bIncludeVideoId)
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("code"), Code);

	if (bIncludeVideoId)
	{
		TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
		Content->SetNumberField(TEXT("vid"), GetActiveVideoId());
		Root->SetObjectField(TEXT("content"), Content);
	}

	return SendJson(Root);
}

bool UVRGameInstance::StartGame(int32 VideoId, int32 LevelId, float ResumeProgress)
{
	if (!MediaPlayer || !Playlist)
	{
		ReportError(TEXT("没有配置MediaPlayer或Playlist。"));
		return false;
	}

	if (VideoId < 0)
	{
		ReportError(TEXT("vid不能小于0。"));
		return false;
	}

	// 为兼容旧接口继续使用参数名LevelId，但它现在就是协议cl，也就是Playlist索引。
	if (LevelId < 0 || LevelId >= Playlist->Num())
	{
		ReportError(TEXT("cl超出Playlist范围。"));
		return false;
	}

	StopProgressTimer();//防止还在发送旧的数据
	bStopping = false;
	bWaitingFinishAck = false;
	bExitFlowStarted = false;//新一轮播放开始，允许之后正常执行一次退出流程
	bFinalVideoCompleted = false;
	bPendingStart = true;
	bWaitingStartAck = false;

	// 如果此前StopGame禁止了重连，而当前命令来自仍然连接着的服务器，则重新允许意外断线重连。
	if (!bDirectMode && WS_IsConnected())
	{
		bSuppressReconnect = false;
		bReconnectExhausted = false; 
		ReconnectAttemptCount = 0;
	}

	PendingVideoId = VideoId;//业务影片ID，例如62
	PendingPlaylistIndex = LevelId;//实际播放索引，例如0、1、2、3
	PendingProgress = FMath::Clamp(ResumeProgress, 0.0f, 1.0f);

	MediaPlayer->Close();
	ActivePlaylistIndex = INDEX_NONE;
	OnStartGameRequested.Broadcast(VideoId, LevelId, PendingProgress);

	if (ChunkPreparationState == EVRChunkPreparationState::Ready)
	{
		return OpenPendingVideo();
	}

	if (ChunkPreparationState == EVRChunkPreparationState::Failed)
	{
		bPendingStart = false;
		ReportError(TEXT("视频Pak准备失败，无法开始播放。"));
		return false;
	}

	// WebSocket命令或Direct启动可能早于下载完成。保留最新Pending参数，挂载完成后自动继续。
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VRCHUNK] 播放请求等待视频Pak。vid=%d cl=%d state=%s"),
		VideoId,
		LevelId,
		*GetVideoChunkState()
	);
	return true;
}

void UVRGameInstance::StopGame(bool bQuitAfterAck)
{
	// 收到 1003 时先停止本地播放，并回复 1004。
	// 退出动作不再单独实现，而是和视频自然结束共用 BeginUnifiedExitFlow。
	bStopping = true;
	bPendingStart = false;
	bWaitingStartAck = false;
	bWaitingFinishAck = false;
	bSuppressReconnect = true;
	StopReconnectTimer();
	StopProgressTimer();

	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}

	OnVRStopGameRequested.Broadcast();

	const bool bAckSent = SendInfo(VRCode::StopGameAck, false);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VREXIT] StopGame: 1004 sent=%s"),
		bAckSent ? TEXT("true") : TEXT("false")
	);

	if (bQuitAfterAck)
	{
		BeginUnifiedExitFlow();
	}
}

void UVRGameInstance::EndGame()
{
	// 防止 OnEndReached 或其他回调在短时间内重复触发结束逻辑。
	if (bExitFlowStarted) return;

	if (bDirectMode)
	{
		bStopping = true;
		StopProgressTimer();
		OnVRGameFinished.Broadcast(GetActiveVideoId());
		BeginUnifiedExitFlow();
		return;
	}

	// 最后一条视频播放完成后，先上传 100% 进度，再发送 1009。
	// 发送完成后不再等待服务器的 1010，由客户端本地计时器自行退出。
	bFinalVideoCompleted = true;
	StopProgressTimer();
	SendProgressInfo();

	bWaitingFinishAck = false;
	OnVRGameFinished.Broadcast(GetActiveVideoId());

	const bool bFinishSent = SendInfo(VRCode::GameFinish, true);
	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VREXIT] EndGame: 1009 sent=%s. Client will quit locally."),
		bFinishSent ? TEXT("true") : TEXT("false")
	);

	BeginUnifiedExitFlow();
}

void UVRGameInstance::QuitGame()
{
	if (UWorld* World = GetWorld())
	{
		UKismetSystemLibrary::QuitGame(
			World,
			World->GetFirstPlayerController(),
			EQuitPreference::Quit,
			false
		);
	}
}

bool UVRGameInstance::NotifyAndroidSystemEndPlay()
{
	UE_LOG(LogTemp, Log, TEXT("[VREXIT] NotifyAndroidSystemEndPlay"));

#if PLATFORM_ANDROID
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VREXIT] JavaEnv is unavailable."));
		return false;
	}

	// 这个 Java 方法由 VR_Midea_UPL.xml 注入 GameActivity。
	// true 表示“方法不存在时不要触发致命错误”，避免再次出现 JNI 崩溃。
	jmethodID MethodId = FJavaWrapper::FindMethod(
		Env,
		FJavaWrapper::GameActivityClassID,
		"AndroidThunkJava_SendBeaconCloseBroadcast",
		"()Z",
		true
	);

	if (!MethodId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VREXIT] Java broadcast method was not injected."));
		return false;
	}

	const jboolean JavaResult = Env->CallBooleanMethod(
		FJavaWrapper::GameActivityThis,
		MethodId
	);

	if (Env->ExceptionCheck())
	{
		// 清除 Java 异常，避免异常状态影响后续 JNI 调用。
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		UE_LOG(LogTemp, Warning, TEXT("[VREXIT] Java exception while sending Beacon Close broadcast."));
		return false;
	}

	const bool bSuccess = JavaResult == JNI_TRUE;
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("[VREXIT] Beacon Close broadcast sent=true"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VREXIT] Beacon Close broadcast sent=false"));
	}
	return bSuccess;
#else
	UE_LOG(LogTemp, Log, TEXT("[VREXIT] Beacon broadcast skipped on non-Android platform."));
	return false;
#endif
}

void UVRGameInstance::BeginUnifiedExitFlow()
{
	// 1003、自然播放完成、1010 等事件可能挨得很近。
	// 这里只允许第一条退出流程真正生效，避免重复广播、重复计时和重复 QuitGame。
	if (bExitFlowStarted)
	{
		UE_LOG(LogTemp, Log, TEXT("[VREXIT] Exit flow is already running; duplicate request ignored."));
		return;
	}

	bExitFlowStarted = true;
	bStopping = true;
	bWaitingFinishAck = false;
	bSuppressReconnect = true;
	StopReconnectTimer();
	StopProgressTimer();

	// 先通知 Beacon，让管理程序提前准备重新回到前台。
	// 即使广播失败，也继续本地退出，避免程序永远卡在结束状态。
	NotifyAndroidSystemEndPlay();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[VREXIT] Return to Beacon and quit in %.2f seconds."),
		QuitDelaySeconds
	);

	ScheduleQuit();
}

void UVRGameInstance::ReturnToBeaconAndQuit()
{
	UE_LOG(LogTemp, Warning, TEXT("[VREXIT] Opening beacon://open and quitting UE."));

#if PLATFORM_ANDROID
	// 将头显管理程序重新拉到前台。
	UKismetSystemLibrary::LaunchURL(TEXT("beacon://open"));
#endif

	// 退出前主动关闭业务 WS，并禁止自动重连。
	WS_Close();
	QuitGame();
}

bool UVRGameInstance::SwitchLevelById(int32 LevelId)
{
	const FVRLevelConfig* Config = FindLevel(LevelId);
	if (!Config || Config->Level.IsNull())
	{
		return false;
	}

	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Config->Level);
	return true;
}

bool UVRGameInstance::PlayVideoByIndex(int32 VideoIndex, float ResumeProgress)
{
	if (!MediaPlayer || !Playlist || VideoIndex < 0 || VideoIndex >= Playlist->Num())
	{
		return false;
	}

	PendingProgress = FMath::Clamp(ResumeProgress, 0.0f, 1.0f);
	bPlaybackConfirmed = false;
	bFinalVideoCompleted = false;

	//请求 MediaPlayer 打开 Playlist 中指定索引的视频。
	//视频打开是异步过程，所以ActivePlaylistIndex在HandlePlaybackResumed中确认。
	return MediaPlayer->OpenPlaylistIndex(Playlist, VideoIndex);
}

void UVRGameInstance::SendProgressInfo()
{
	// 客户端内部统一使用 0～1。
	// 例如 0.7 代表视频播放到了 70%。
	const float Clp = bFinalVideoCompleted ? 1.0f : GetCurrentVideoProgress();
	const float Gp = FMath::Clamp(CalculateGlobalProgress(), 0.0f, 1.0f);
	
	// 蓝图事件仍然广播 0～1，避免影响项目里已有的蓝图逻辑。
	OnVRProgressUpdated.Broadcast(GetActiveVideoId(), ActivePlaylistIndex, Clp, Gp);
	
	// 省博中控实际使用的是 0～100。
	// 所以只在发 JSON 前乘以 100。
	const float ClpPercent = Clp * 100.0f;
	const float GpPercent = Gp * 100.0f;

	TSharedPtr<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetNumberField(TEXT("vid"), GetActiveVideoId());//业务影片ID，原样返回服务器下发值
	Content->SetNumberField(TEXT("cl"), ActivePlaylistIndex);//当前真正播放的Playlist索引
	Content->SetNumberField(TEXT("clp"), ClpPercent);
	Content->SetNumberField(TEXT("gp"), GpPercent);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("code"), VRCode::UploadProgress);
	Root->SetObjectField(TEXT("content"), Content);
	SendJson(Root);
}

float UVRGameInstance::GetCurrentVideoProgress() const
{
	if (!MediaPlayer) return 0.0f;

	const double Duration = MediaPlayer->GetDuration().GetTotalSeconds();
	if (Duration <= 0.0) return 0.0f;

	return FMath::Clamp(
		static_cast<float>(MediaPlayer->GetTime().GetTotalSeconds() / Duration),
		0.0f,
		1.0f
	);
}

float UVRGameInstance::CalculateGlobalProgress_Implementation() const
{
	const float CurrentProgress = bFinalVideoCompleted ? 1.0f : GetCurrentVideoProgress();
	if (!Playlist || ActivePlaylistIndex < 0 || ActivePlaylistIndex >= Playlist->Num()) return CurrentProgress;

	// 没有完整时长配置时按条目等权，保证中途开始播放时总体进度仍然正确递增。
	if (PlaylistDurationsSeconds.Num() != Playlist->Num())
	{
		return FMath::Clamp((ActivePlaylistIndex + CurrentProgress) / Playlist->Num(), 0.0f, 1.0f);
	}

	double TotalSeconds = 0.0;
	double PlayedSeconds = 0.0;
	for (int32 Index = 0; Index < PlaylistDurationsSeconds.Num(); ++Index)
	{
		const double Duration = FMath::Max(static_cast<double>(PlaylistDurationsSeconds[Index]), 0.0);
		TotalSeconds += Duration;
		if (Index < ActivePlaylistIndex) PlayedSeconds += Duration;
		else if (Index == ActivePlaylistIndex) PlayedSeconds += Duration * CurrentProgress;
	}

	return TotalSeconds > 0.0 ? FMath::Clamp(static_cast<float>(PlayedSeconds / TotalSeconds), 0.0f, 1.0f) : CurrentProgress;
}

int32 UVRGameInstance::GetActiveVideoId() const
{
	// 这是服务器业务影片ID，不是MediaPlayer的0、1、2、3索引。
	return ActiveVideoId;
}

int32 UVRGameInstance::GetActivePlaylistIndex() const
{
	return ActivePlaylistIndex;
}

int32 UVRGameInstance::GetActiveLevelId() const
{
	// 兼容旧蓝图函数名，返回当前协议cl/Playlist索引。
	return ActivePlaylistIndex;
}

bool UVRGameInstance::AreVideoChunksReady() const
{
	return ChunkPreparationState == EVRChunkPreparationState::Ready;
}

FString UVRGameInstance::GetVideoChunkState() const
{
	switch (ChunkPreparationState)
	{
	case EVRChunkPreparationState::NotStarted:
		return TEXT("NotStarted");
	case EVRChunkPreparationState::UpdatingManifest:
		return TEXT("UpdatingManifest");
	case EVRChunkPreparationState::Downloading:
		return TEXT("Downloading");
	case EVRChunkPreparationState::Mounting:
		return TEXT("Mounting");
	case EVRChunkPreparationState::Ready:
		return TEXT("Ready");
	case EVRChunkPreparationState::Failed:
		return TEXT("Failed");
	default:
		return TEXT("Unknown");
	}
}

float UVRGameInstance::GetChunkDownloadProgress() const
{
	if (ChunkPreparationState == EVRChunkPreparationState::Ready)
	{
		return 1.0f;
	}

	const TSharedPtr<FChunkDownloader> Downloader = FChunkDownloader::Get();

	if (!Downloader.IsValid())
	{
		return 0.0f;
	}

	const FChunkDownloader::FStats& Stats = Downloader->GetLoadingStats();

	if (Stats.TotalBytesToDownload <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>(
			static_cast<double>(Stats.BytesDownloaded) /
			static_cast<double>(Stats.TotalBytesToDownload)
		),
		0.0f,
		1.0f
	);
}

void UVRGameInstance::HandleMapLoaded(UWorld* LoadedWorld)
{
	// 协议cl已经改为Playlist索引，因此地图加载不再负责继续StartGame播放流程。
	if (LoadedWorld)
	{
		UE_LOG(LogTemp, Verbose, TEXT("Map loaded without protocol playback switching: %s"), *LoadedWorld->GetMapName());
	}
}

void UVRGameInstance::HandleMediaOpened(FString OpenedUrl)
{
	if (!MediaPlayer) return;

	// 自动切到播放列表下一条时，不会经过PlayVideoByIndex，所以每次打开都重新允许播放确认。
	bPlaybackConfirmed = false;

	const int32 OpenedIndex = MediaPlayer->GetPlaylistIndex();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Media opened: PlaylistIndex=%d, URL=%s"),
		OpenedIndex,
		
		
		*OpenedUrl
	);

	OnVRMediaOpened.Broadcast(
		OpenedUrl,
		OpenedIndex != INDEX_NONE ? OpenedIndex : PendingPlaylistIndex
	);

	const double Duration = MediaPlayer->GetDuration().GetTotalSeconds();
	if (PendingProgress > 0.0f && Duration > 0.0)
	{
		MediaPlayer->Seek(FTimespan::FromSeconds(Duration * PendingProgress));
	}

	/*if (!MediaPlayer->Play())
	{
		ReportError(TEXT("视频打开成功，但播放失败。"));
	}*/
}

void UVRGameInstance::HandleMediaOpenFailed(FString FailedUrl)
{
	bPendingStart = false;
	bWaitingStartAck = false;
	ActivePlaylistIndex = INDEX_NONE;
	ReportError(FString::Printf(TEXT("视频打开失败：%s"), *FailedUrl));
}

void UVRGameInstance::HandlePlaybackResumed()//调用Play()并不能百分之百说明视频已经真正开始播放，这里才代表播放已确认
{
	if (!MediaPlayer || !MediaPlayer->IsPlaying() || bPlaybackConfirmed) return;

	bPlaybackConfirmed = true;

	const int32 PlayingIndex = MediaPlayer->GetPlaylistIndex();
	if (PlayingIndex != INDEX_NONE)
	{
		ActivePlaylistIndex = PlayingIndex;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Playback started: VideoId=%d, PlaylistIndex=%d"),
		ActiveVideoId,
		ActivePlaylistIndex
	);

	OnVRPlaybackStarted.Broadcast(ActiveVideoId, ActivePlaylistIndex);
	OnVRVideoChanged.Broadcast(ActivePlaylistIndex);

	if (bWaitingStartAck)
	{
		bWaitingStartAck = false;
		bPendingStart = false;
		PendingProgress = 0.0f;

		if (!bDirectMode)
		{
			SendInfo(VRCode::StartGameAck, true);
			StartProgressTimer();
		}
	}
}

void UVRGameInstance::HandleEndReached()
{
	if (bStopping || bWaitingFinishAck || !Playlist || !MediaPlayer) return;

	// 使用最后一次真正进入播放状态时保存的索引，不读取可能已经自动前进的MediaPlayer内部索引。
	const int32 FinishedIndex = ActivePlaylistIndex;
	const int32 LastIndex = Playlist->Num() - 1;

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("Video ended: FinishedIndex=%d, MediaPlayerIndex=%d, LastIndex=%d"),
		FinishedIndex,
		MediaPlayer->GetPlaylistIndex(),
		LastIndex
	);

	if (FinishedIndex >= 0 && FinishedIndex < LastIndex)
	{
		if (!bDirectMode)
		{
			bFinalVideoCompleted = true;
			SendProgressInfo();
			bFinalVideoCompleted = false;
		}

		PendingProgress = 0.0f;
		return;//Playlist会自动打开下一条
	}

	EndGame();
}

void UVRGameInstance::BindMediaEvents()
{
	if (!MediaPlayer) return;

	MediaPlayer->PlayOnOpen = true;
	MediaPlayer->Shuffle = false;//洗牌
	MediaPlayer->SetLooping(false);
	MediaPlayer->OnMediaOpened.AddUniqueDynamic(this, &UVRGameInstance::HandleMediaOpened);//视频文件打开成功
	MediaPlayer->OnMediaOpenFailed.AddUniqueDynamic(this, &UVRGameInstance::HandleMediaOpenFailed);//视频文件打开失败
	MediaPlayer->OnPlaybackResumed.AddUniqueDynamic(this, &UVRGameInstance::HandlePlaybackResumed);//视频正在播放
	MediaPlayer->OnEndReached.AddUniqueDynamic(this, &UVRGameInstance::HandleEndReached);//视频播放完毕
}

void UVRGameInstance::UnbindMediaEvents()
{
	if (!MediaPlayer) return;

	MediaPlayer->OnMediaOpened.RemoveDynamic(this, &UVRGameInstance::HandleMediaOpened);
	MediaPlayer->OnMediaOpenFailed.RemoveDynamic(this, &UVRGameInstance::HandleMediaOpenFailed);
	MediaPlayer->OnPlaybackResumed.RemoveDynamic(this, &UVRGameInstance::HandlePlaybackResumed);
	MediaPlayer->OnEndReached.RemoveDynamic(this, &UVRGameInstance::HandleEndReached);
}

bool UVRGameInstance::OpenPendingVideo()
{
	bWaitingStartAck = true;//正在等待视频真正开始播放，之后需要向服务器发送1002确认
	ActiveVideoId = PendingVideoId;//业务影片ID在整个Playlist播放期间保持不变

	if (!PlayVideoByIndex(PendingPlaylistIndex, PendingProgress))
	{
		bPendingStart = false;
		bWaitingStartAck = false;
		ActiveVideoId = INDEX_NONE;
		ActivePlaylistIndex = INDEX_NONE;
		ReportError(TEXT("打开Playlist视频失败。"));
		return false;
	}

	return true;
}

void UVRGameInstance::StartProgressTimer()//定时发送数据到服务器
{
	if (UWorld* World = GetWorld())
	{
		const float Interval = FMath::Max(UploadIntervalSeconds, 0.1f);
		World->GetTimerManager().SetTimer(
			ProgressTimer,
			this,
			&UVRGameInstance::SendProgressInfo,
			Interval,
			true
		);
	}
}

void UVRGameInstance::StopProgressTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProgressTimer);
	}
}

void UVRGameInstance::ScheduleQuit()
{
	const float SafeDelaySeconds = FMath::Max(QuitDelaySeconds, 0.0f);

	if (SafeDelaySeconds <= 0.0f)
	{
		ReturnToBeaconAndQuit();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		// 重新安排前先清理旧计时器，保证只执行一次。
		World->GetTimerManager().ClearTimer(QuitTimer);
		World->GetTimerManager().SetTimer(
			QuitTimer,
			this,
			&UVRGameInstance::ReturnToBeaconAndQuit,
			SafeDelaySeconds,
			false
		);
	}
	else
	{
		// 正常情况下 GameInstance 一定有 World；这里是保险兜底。
		ReturnToBeaconAndQuit();
	}
}

void UVRGameInstance::ReportError(const FString& Message)
{
	UE_LOG(LogTemp, Error, TEXT("%s"), *Message);
	OnWebSocketError.Broadcast(Message);
	OnVRMediaError.Broadcast(Message);
}

bool UVRGameInstance::SendJson(const TSharedPtr<FJsonObject>& JsonObject)
{
	if (!JsonObject.IsValid() || !WS_IsConnected()) return false;

	FString Text;//接收最终生成的 JSON 字符串
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer)) return false;//JSON 对象转字符串

	WebSocket->Send(Text);
	return true;
}

const FVRLevelConfig* UVRGameInstance::FindLevel(int32 LevelId) const
{
	return LevelConfigs.FindByPredicate([LevelId](const FVRLevelConfig& Config)
	{
		return Config.LevelId == LevelId;
	});
}

bool UVRGameInstance::IsCurrentLevel(int32 LevelId) const
{
	const FVRLevelConfig* Config = FindLevel(LevelId);

	if (!Config || Config->Level.IsNull())
	{
		return false;
	}

	// true：移除编辑器运行时添加的 UEDPIE_0_ 前缀
	const FString CurrentLevelName =
		UGameplayStatics::GetCurrentLevelName(this, true);

	const FString TargetLevelName =
		Config->Level.ToSoftObjectPath().GetAssetName();

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("CurrentLevel=%s, TargetLevel=%s"),
		*CurrentLevelName,
		*TargetLevelName
	);

	return CurrentLevelName.Equals(
		TargetLevelName,
		ESearchCase::IgnoreCase
	);
}
