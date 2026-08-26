#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "IWebSocket.h"
#include "VRGameInstance.generated.h"

class FJsonObject;
class UMediaPlayer;
class UMediaPlaylist;
class UWorld;

USTRUCT(BlueprintType)
struct FVRLevelConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VR|Level")
	int32 LevelId = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VR|Level")
	TSoftObjectPtr<UWorld> Level;
};

/*
 * 动态多播委托，这里只是想让蓝图也可以执行相关逻辑，增加自由度
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWebSocketConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebSocketMessageReceived, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWebSocketError, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWebSocketClosed, int32, StatusCode, const FString&, Reason, bool, bWasClean);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnVRStartGameRequested, int32, VideoId, int32, LevelId, float, ResumeProgress);//在收到 1001 且 start game 后广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVRLevelLoaded, int32, LevelId, const FString&, LevelName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVRMediaOpened, const FString&, OpenedUrl, int32, VideoId);//兼容旧引脚名；这里实际返回Playlist索引
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVRPlaybackStarted, int32, VideoId, int32, LevelId);//兼容旧引脚名；LevelId现在表示Playlist索引/cl
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVRVideoChanged, int32, VideoId);//兼容旧引脚名；这里实际广播当前Playlist索引
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnVRProgressUpdated, int32, VideoId, int32, LevelId, float, LevelProgress, float, GlobalProgress);//兼容旧引脚名；LevelId现在表示Playlist索引/cl
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVRStopGameRequested);//游戏正在执行停止流程
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVRGameFinished, int32, VideoId);//整套视频播放流程已经正常完成。
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVRProtocolAckReceived, int32, Code);//收到了某个协议确认消息
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVRMediaError, const FString&, ErrorMessage);//视频打开或播放发生错误。

UCLASS(BlueprintType, Blueprintable)
class CHUNKTEST_API UVRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// WebSocket
	UFUNCTION(BlueprintCallable, Category="VR|WebSocket")
	void WS_Connect(const FString& InServerURL);

	UFUNCTION(BlueprintCallable, Category="VR|WebSocket")
	void WS_SendMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category="VR|WebSocket")
	void WS_Close();

	UFUNCTION(BlueprintPure, Category="VR|WebSocket")
	bool WS_IsConnected() const;

	UFUNCTION(BlueprintCallable, Category="VR|WebSocket")
	bool ConnectUsingConfig();//连接 WS

	UFUNCTION(BlueprintCallable, Category="VR|WebSocket")
	void ReloadVRConfiguration();

	UFUNCTION(BlueprintPure, Category="VR|WebSocket")
	FString GetConfiguredWebSocketURL() const;//拼接字符串

	UFUNCTION(BlueprintPure, Category="VR|Device")
	FString GetPlatformDeviceId() const;

	// 兼容同事旧函数名。实际返回当前平台设备ID。
	UFUNCTION(BlueprintPure, Category="VR|Device")
	FString GetAndroidADeviceID() const;

	UFUNCTION(BlueprintPure, Category="VR|Device")
	FString GetResolvedDeviceId() const;

	// 中控协议
	UFUNCTION(BlueprintCallable, Category="VR|Protocol")
	void HandleProtocolMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category="VR|Protocol")
	void StartGameFromMessage(const FString& Message);

	UFUNCTION(BlueprintCallable, Category="VR|Protocol")
	bool SendInfo(int32 Code, bool bIncludeVideoId=false);

	// 游戏与播放
	// 为兼容现有蓝图保留参数名 LevelId；它现在表示协议 cl，也就是 Playlist 索引。
	UFUNCTION(BlueprintCallable, Category="VR|Game")
	bool StartGame(int32 VideoId, int32 LevelId, float ResumeProgress=0.0f);

	UFUNCTION(BlueprintCallable, Category="VR|Game")
	void StopGame(bool bQuitAfterAck=true);

	UFUNCTION(BlueprintCallable, Category="VR|Game")
	void EndGame();

	UFUNCTION(BlueprintCallable, Category="VR|Game")
	void QuitGame();

	// 通知头显里的 Beacon 管理程序：当前 VR 播放即将结束。
	// Android 会发送一条 message=Close 的广播；非 Android 平台只返回 false。
	UFUNCTION(BlueprintCallable, Category="VR|Android")
	bool NotifyAndroidSystemEndPlay();

	UFUNCTION(BlueprintCallable, Category="VR|Level")
	bool SwitchLevelById(int32 LevelId);

	UFUNCTION(BlueprintCallable, Category="VR|Media")
	bool PlayVideoByIndex(int32 VideoIndex, float ResumeProgress=0.0f);

	UFUNCTION(BlueprintCallable, Category="VR|Progress")
	void SendProgressInfo();

	UFUNCTION(BlueprintPure, Category="VR|Progress")
	float GetCurrentVideoProgress() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="VR|Progress")
	float CalculateGlobalProgress() const;
	virtual float CalculateGlobalProgress_Implementation() const;

	UFUNCTION(BlueprintPure, Category="VR|State")
	int32 GetActiveVideoId() const;

	UFUNCTION(BlueprintPure, Category="VR|State")
	int32 GetActivePlaylistIndex() const;

	// 兼容旧蓝图函数名，当前返回值同样是 Playlist 索引（协议 cl）。
	UFUNCTION(BlueprintPure, Category="VR|State")
	int32 GetActiveLevelId() const;

	UFUNCTION(BlueprintPure, Category="VR|Chunk")
	bool AreVideoChunksReady() const;

	UFUNCTION(BlueprintPure, Category="VR|Chunk")
	FString GetVideoChunkState() const;
	
	UFUNCTION(BlueprintPure, Category="VR|Chunk")
	float GetChunkDownloadProgress() const;
	
	// UE回调，同时保留蓝图可调用
	UFUNCTION(BlueprintCallable, Category="VR|Callbacks")
	void HandleMapLoaded(UWorld* LoadedWorld);

	UFUNCTION(BlueprintCallable, Category="VR|Callbacks")
	void HandleMediaOpened(FString OpenedUrl);

	UFUNCTION(BlueprintCallable, Category="VR|Callbacks")
	void HandleMediaOpenFailed(FString FailedUrl);

	UFUNCTION(BlueprintCallable, Category="VR|Callbacks")
	void HandlePlaybackResumed();

	UFUNCTION(BlueprintCallable, Category="VR|Callbacks")
	void HandleEndReached();

	// 资源
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VR|Media")
	TObjectPtr<UMediaPlayer> MediaPlayer = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VR|Media")
	TObjectPtr<UMediaPlaylist> Playlist = nullptr;

	// 每项对应 Playlist 中同索引视频的时长（秒），远程配置 videoDurations 可覆盖。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VR|Media")
	TArray<float> PlaylistDurationsSeconds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VR|Level", meta=(TitleProperty="LevelId"))
	TArray<FVRLevelConfig> LevelConfigs;

	// DefaultGame.ini
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	bool bAutoConnectOnInit = true;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString ServerURL;

	// 为空：自动取当前平台设备ID；不为空：使用这里的值，例如T0。
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString DeviceIdOverride;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	int32 ClientType = 1;//客户端类型编号，但目前不确定具体的业务含义

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	bool bUseSignedHandshake = true;//连接 WS 时，要不要在 URL 后面添加设备信息和签名

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString SignaturePrefix;// MD5 签名前缀

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString SignatureSuffix;//后缀

	UPROPERTY(BlueprintReadOnly, Category="VR|Config", meta=(ClampMin="0.1"))
	float UploadIntervalSeconds = 1.0f;//客户端发送频率

	UPROPERTY(BlueprintReadOnly, Category="VR|Config", meta=(ClampMin="0.0"))
	float QuitDelaySeconds = 2.0f;//发送确认与Close广播后，等待多久再返回Beacon并退出

	// 远程启动配置
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	bool bEnableRemoteConfig = true;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString ConfigURL;//远程配置文件地址

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	float RequestTimeoutSeconds = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	bool bDirectMode = false;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	int32 DirectVideoId = 0;

	// 为兼容远程配置字段 levelId 保留名称；当前含义是 Playlist 索引（协议 cl）。
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	int32 DirectLevelId = 0;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	float DirectProgress = 0.0f;


	// 视频 Pak 配置。程序先从 DefaultGame.ini 读取；远程 JSON 中存在同名字段时再覆盖。
	// 地址必须包含 Build 文件夹，例如：http://192.168.2.111:8000/VRVideoBuild01
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	FString PakBuildUrl;

	// 启动时自动下载并挂载的全部视频 Chunk。
	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	TArray<int32> ChunkIds;

	UPROPERTY(BlueprintReadOnly, Category="VR|Config", meta=(ClampMin="1"))
	int32 ChunkTargetDownloadsInFlight = 1;//最多允许多少个下载任务同时进行

	UPROPERTY(BlueprintReadOnly, Category="VR|Config")
	int32 ChunkDownloadPriority = 1;//这批下载任务的调度优先级

	// 蓝图事件
	UPROPERTY(BlueprintAssignable, Category="VR|WebSocket")
	FOnWebSocketConnected OnWebSocketConnected;

	UPROPERTY(BlueprintAssignable, Category="VR|WebSocket")
	FOnWebSocketMessageReceived OnWebSocketMessageReceived;

	UPROPERTY(BlueprintAssignable, Category="VR|WebSocket")
	FOnWebSocketError OnWebSocketError;

	UPROPERTY(BlueprintAssignable, Category="VR|WebSocket")
	FOnWebSocketClosed OnWebSocketClosed;

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRStartGameRequested OnStartGameRequested;//在 StartGame 中广播，不代表已经播放，只是代表进入了执行流程

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRLevelLoaded OnVRLevelLoaded;//保留给手动地图切换；协议 StartGame 不再触发地图切换

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRMediaOpened OnVRMediaOpened;//在尝试打开视频后广播

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRPlaybackStarted OnVRPlaybackStarted;//播放时广播

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRVideoChanged OnVRVideoChanged;//广播正在播放的视频索引

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRProgressUpdated OnVRProgressUpdated;//进程更改时进行广播

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRStopGameRequested OnVRStopGameRequested;//停止游戏时广播

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRGameFinished OnVRGameFinished;//

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRProtocolAckReceived OnVRProtocolAckReceived;

	UPROPERTY(BlueprintAssignable, Category="VR|Events")
	FOnVRMediaError OnVRMediaError;

private:
	enum class EVRChunkPreparationState : uint8
	{
		NotStarted,					//还未开始
		UpdatingManifest,			//正在更新资源清单
		Downloading,				//正在下载 Pak
		Mounting,					//正在挂载 Pak
		Ready,						//全部准备完成，可以播放
		Failed						//中途失败
	};

	bool LoadWebSocketConfig();//从 config 中获取储存的各项数值，变量定义在上方
	void DownloadRemoteConfig();//从远端地址下载配置文件
	bool ApplyRemoteConfigText(const FString& Text, bool bSaveCache);//读取远端文件并设置当前变量
	void StartFromCurrentConfig();
	FString GetRemoteConfigCachePath() const;

	// ChunkDownloader：应用最终配置后立即更新清单、下载并挂载所有视频 Pak。
	bool ParsePakBuildUrl(const FString& InUrl, FString& OutCdnBaseUrl, FString& OutContentBuildId) const;//作用是吧完整地址拆分成服务器地址和Build名称
	bool ApplyChunkBaseUrlToRuntimeConfig(const FString& CdnBaseUrl) const;//覆盖运行时的清单
	void StartChunkPreparation();//根据最终确定的配置，正式启动视频 Pak 的准备流程
	void HandleChunkManifestUpdated(bool bSuccess, bool bCachedBuildLoaded);//回调函数，处理清单更新
	void StartChunkDownload();
	void HandleChunkDownloadFinished(bool bSuccess);
	void StartChunkMount();//挂载
	void HandleChunkMountFinished(bool bSuccess);//挂载完成
	bool AreAllConfiguredChunksCachedOrMounted() const;
	bool ValidateConfiguredChunksAgainstManifest() const;
	void ContinuePendingPlaybackAfterChunksReady();
	void MarkChunkPreparationFailed(const FString& Reason);//Debug 函数，用于返回错误的

	void ConnectWebSocketInternal(const FString& URL);
	void ScheduleReconnect();//计划重连
	void AttemptReconnect();
	void StopReconnectTimer();

	void BindMediaEvents();//绑定 Media 相关的回调函数
	void UnbindMediaEvents();
	bool OpenPendingVideo();//打开准备的视频
	void StartProgressTimer();
	void StopProgressTimer();

	// StopGame 和视频自然结束都从这里进入同一套退出流程：
	// 通知 Beacon -> 等待一小段时间 -> 打开 beacon://open -> 退出 UE。
	void BeginUnifiedExitFlow();
	void ScheduleQuit();//按照 QuitDelaySeconds 延迟执行 ReturnToBeaconAndQuit
	void ReturnToBeaconAndQuit();

	void ReportError(const FString& Message);

	bool SendJson(const TSharedPtr<FJsonObject>& JsonObject);
	const FVRLevelConfig* FindLevel(int32 LevelId) const;
	bool IsCurrentLevel(int32 LevelId) const;

	TSharedPtr<IWebSocket> WebSocket;//由于这个东西不是 UObject ，所以不能靠 UPROPERTY 让 GC 管，所以用共享指针
	FTimerHandle ProgressTimer;
	FTimerHandle QuitTimer;
	FTimerHandle ReconnectTimer;


	EVRChunkPreparationState ChunkPreparationState = EVRChunkPreparationState::NotStarted;
	bool bChunkDownloaderInitialized = false;//记录 ChunkDownloader 是否初始化

	FString LastWebSocketURL;//保存上一次的 URL 地址
	static constexpr int32 FastReconnectAttempts = 5;//前五次快速重连
	static constexpr float ReconnectIntervalSeconds = 5.0f;//快速阶段五秒一次
	static constexpr float PersistentReconnectIntervalSeconds = 10.0f;//之后每十秒持续重连
	int32 ReconnectAttemptCount = 0;//尝试重连次数
	int32 WebSocketGeneration = 0;//忽略被替换掉的旧连接延迟回调

	int32 ActiveVideoId = INDEX_NONE;//服务器下发的业务影片ID，例如62；回传时保持不变
	int32 ActivePlaylistIndex = INDEX_NONE;//当前真正播放的Playlist索引，也就是协议cl
	int32 PendingVideoId = INDEX_NONE;//等待生效的业务影片ID
	int32 PendingPlaylistIndex = INDEX_NONE;//等待打开的Playlist索引，也就是协议cl
	float PendingProgress = 0.0f;

	bool bPendingStart = false;//有一个启动任务正在等待完成
	bool bWaitingStartAck = false;//等待视频真正开始，然后发送 1002
	bool bWaitingFinishAck = false;//兼容旧状态；现在1009发送后不再等待1010才退出
	bool bStopping = false;//当前正在停止，避免视频结束回调又触发 EndGame
	bool bPlaybackConfirmed = false;//这次视频是否已经确认真正进入播放状态
	bool bFinalVideoCompleted = false;//上传进度时，是否强制让当前视频进度等于 1
	bool bStartupApplied = false;//避免远程、缓存、ini重复启动
	bool bSuppressReconnect = false;//主动关闭、停止游戏时禁止自动重连
	bool bReconnectExhausted = false;//避免超过次数后重复报错

	// 防止 1003、视频结束回调、1010 等多个事件同时重复启动退出流程。
	bool bExitFlowStarted = false;
	bool bShuttingDown = false;
};
