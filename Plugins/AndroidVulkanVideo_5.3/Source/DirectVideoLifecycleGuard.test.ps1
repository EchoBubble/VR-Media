$rendererPath = Join-Path $PSScriptRoot 'VulkanVideoMeshComponent/Private/DirectVideoMeshRenderer.cpp'
$renderer = Get-Content -Raw -LiteralPath $rendererPath

$flush = [regex]::Match($renderer, 'void UDirectVideoMeshRendererComponent::RequestFlush\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $flush.Success) { throw 'RequestFlush was not found' }
if ($flush.Groups['body'].Value -notmatch '(?m)^\s*PreparedSample\.Reset\(\)\s*;') {
    throw 'RequestFlush must release PreparedSample during a media source switch'
}

$sampleHeaderPath = Join-Path $PSScriptRoot 'AndroidVulkanVideo/Public/AndroidVulkanTextureSample.h'
$sampleHeader = Get-Content -Raw -LiteralPath $sampleHeaderPath
if ($sampleHeader -match 'RenderState GetRenderState\(\)\s*\{') {
    throw 'GetRenderState must be implemented out-of-line so native frame readiness can be checked'
}

$playerPath = Join-Path $PSScriptRoot 'AndroidVulkanVideo/Private/AndroidVulkanMediaPlayer.cpp'
$player = Get-Content -Raw -LiteralPath $playerPath
$trackSwitch = [regex]::Match($player, 'case EFeatureFlag::IsTrackSwitchSeamless:\s*return\s+(?<value>true|false)\s*;')
if (-not $trackSwitch.Success -or $trackSwitch.Groups['value'].Value -ne 'false') {
    throw 'Track switches must request the facade flush path'
}

$audioPath = Join-Path $PSScriptRoot 'AndroidVulkanVideo/Private/UnrealAudioOut.cpp'
$audio = Get-Content -Raw -LiteralPath $audioPath
$audioClose = [regex]::Match($audio, 'void UnrealAudioOut::close\(\)\s*\{(?<body>.*?)\n\}', [Text.RegularExpressions.RegexOptions]::Singleline)
if (-not $audioClose.Success) { throw 'UnrealAudioOut::close was not found' }
if ($audioClose.Groups['body'].Value -match 'DestroyComponent\(') {
    throw 'Source switches must not destroy the media sound component'
}
if ($audio -notmatch 'UnrealAudioOut::~UnrealAudioOut\(\)\s*\{\s*DestroyCreatedMediaSoundComponent\(\)') {
    throw 'The plugin-created media sound component must be destroyed with UnrealAudioOut'
}

Write-Output 'Direct video lifecycle regression checks passed.'
