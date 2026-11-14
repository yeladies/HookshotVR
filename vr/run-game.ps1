param(
    [string]$InstallationRoot = (Join-Path $env:USERPROFILE 'Games/Hookshot'),
    [ValidateRange(5, 300)][int]$Seconds = 60,
    [ValidateRange(4, 14)][int]$MaximumWorkingSetGiB = 11,
    [switch]$Window,
    [switch]$PrepareAllShaders,
    [switch]$SnapshotStableSaves,
    [switch]$LoadTestSave,
    [switch]$ExperimentalXrRenderer,
    [switch]$XrNegotiation,
    [switch]$XrSessionProbe,
    [switch]$XrPreview,
    [switch]$ImmersivePreview,
    [switch]$WideFov,
    [switch]$LowLatencyPreview,
    [ValidateSet("720p","1080p")][string]$PreviewResolution="720p",
    [ValidateRange(0,25)][int]$FovMarginPercent=0,
    [switch]$TemporalStereo,
    [switch]$ImmersiveReadbackTest,
    [switch]$PreviewReadbackTest,
    [switch]$CameraCapture,
    [switch]$SceneCapture,
    [switch]$ImageLineage,
    [switch]$CameraUniformAudit,
    [switch]$ShaderUniformAudit,
    [switch]$ComputeShaderUniformAudit,
    [switch]$EyeImageProbe,
    [switch]$EyeUniformProbe,
    [switch]$BaselineDrawPreflight,
    [switch]$IsolatedDrawProbe,
    [switch]$GpuEyeMatrixProbe,
    [switch]$GpuRootAliasProbe,
    [switch]$ShiftedDrawProbe,
    [switch]$TranslatedDrawProbe,
    [switch]$TranslatedColorDrawProbe,
    [ValidateRange(.001,8)][float]$ViewShiftUnits=.01,
    [switch]$DepthSceneCapture,
    [switch]$DepthSceneStream,
    [string]$DepthCapturePath,
    [string]$PreviewCaptureDirectory,
    [string]$StopSignal,
    [ValidateRange(100,1000)][int]$DepthIntervalMs=200,
    [ValidateRange(1,300)][int]$DepthMaximumFrames=300,
    [string]$SpirvGrammar,
    [switch]$Audio,
    [string]$OpenXRLoader
)
$ErrorActionPreference = 'Stop'
if(($LowLatencyPreview -or $PreviewResolution -ne '720p' -or $FovMarginPercent -gt 0) -and !($ImmersivePreview -or $ImmersiveReadbackTest)){throw 'Preview quality and FOV margin require an isolated immersive mode.'}
if($FovMarginPercent -gt 0){$WideFov=$true}
$previewHD=($ImmersivePreview -or $ImmersiveReadbackTest) -and $PreviewResolution -eq '1080p'
$env:RX_VK_XR_LOW_LATENCY_READBACK=if($LowLatencyPreview){'1'}else{'0'}
$env:RX_VK_XR_PREVIEW_HD=if($previewHD){'1'}else{'0'}
$env:RX_VK_XR_FOV_TANGENT_SCALE=(1+$FovMarginPercent/100.0).ToString([Globalization.CultureInfo]::InvariantCulture)

if($GpuRootAliasProbe){if(!$IsolatedDrawProbe){throw 'GPU root alias probe requires the isolated non-XR baseline.'};if($TranslatedColorDrawProbe){throw 'GPU root alias probe requires the translated depth diagnostic.'};$TranslatedDrawProbe=$true}
if($TranslatedColorDrawProbe){if(!$IsolatedDrawProbe){throw 'Translated color probe requires the isolated non-XR baseline.'};$TranslatedDrawProbe=$true}
if($PSBoundParameters.ContainsKey('ViewShiftUnits') -and !$TranslatedDrawProbe){throw 'View shift units require the translated non-XR probe.'}
if($TranslatedDrawProbe){if(!$IsolatedDrawProbe){throw 'Translated draw probe requires the isolated non-XR baseline.'};$ShiftedDrawProbe=$true}
if($ShiftedDrawProbe -and !$IsolatedDrawProbe){throw 'Lens-shift draw probe requires the isolated non-XR baseline.'}
if($ShiftedDrawProbe){$GpuEyeMatrixProbe=$true}
if($GpuEyeMatrixProbe -and !$IsolatedDrawProbe){throw 'GPU matrix transport probe requires the isolated non-XR draw baseline.'}
if($PreviewCaptureDirectory -and !$ImmersiveReadbackTest){throw 'Private image capture requires the non-XR immersive readback test.'}
if(($WideFov -or $TemporalStereo) -and !($ImmersivePreview -or $ImmersiveReadbackTest)){throw 'Wide FOV/stereo require an isolated immersive mode.'}
if($TemporalStereo){$WideFov=$true}
if($ImmersivePreview -or $ImmersiveReadbackTest){
    if($Seconds -lt 120){throw 'The immersive test needs at least 120 seconds including save loading.'}
    if($ImmersivePreview -and $ImmersiveReadbackTest){throw 'Synthetic head motion must not open XR.'}
    if($DepthSceneCapture -or $DepthSceneStream -or $IsolatedDrawProbe -or $EyeImageProbe -or $EyeUniformProbe){throw 'Immersive camera needs its own isolated run.'}
    if($ImmersiveReadbackTest -and ($XrPreview -or $XrSessionProbe -or $XrNegotiation)){throw 'Synthetic head motion cannot be combined with XR.'}
    $SceneCapture=$true
    if($ImmersivePreview){$XrPreview=$true}else{$PreviewReadbackTest=$true}
}
if($DepthSceneStream){$DepthSceneCapture=$true}
if($DepthSceneCapture){if($Seconds -lt 100 -or $IsolatedDrawProbe -or $XrPreview -or $XrSessionProbe -or $XrNegotiation){throw 'Depth capture needs a separate non-XR run of at least 100 seconds.'};$SceneCapture=$true}
if ($IsolatedDrawProbe) {
    if($XrPreview -or $XrSessionProbe -or $XrNegotiation -or $EyeImageProbe -or $EyeUniformProbe -or $ComputeShaderUniformAudit){throw 'The isolated unchanged draw probe requires its own non-XR run.'}
    $BaselineDrawPreflight=$true
    if(!$SpirvGrammar){$SpirvGrammar=Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions/v2.1.0/HookshotBuilder/tools/cpm/spirv-headers/vulkan-sdk-1.4.341.0/include/spirv/unified1/spirv.core.grammar.json'}
    $SpirvGrammar=(Resolve-Path -LiteralPath $SpirvGrammar).Path
}
if ($BaselineDrawPreflight) { $ShaderUniformAudit=$true; $ImageLineage=$true }
if ($ComputeShaderUniformAudit) { $ShaderUniformAudit = $true }
if ($ImageLineage -or $CameraUniformAudit -or $ShaderUniformAudit -or $EyeImageProbe -or $EyeUniformProbe) { $SceneCapture = $true }
if ($SceneCapture) { $CameraCapture = $true }
if ($LoadTestSave -and !$SnapshotStableSaves) { throw 'Scripted save loading requires a private save snapshot.' }
function Read-ExperimentalLog {
    $stream = [IO.File]::Open((Join-Path $runRoot 'stderr.txt'), [IO.FileMode]::Open,
                              [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    $reader = [IO.StreamReader]::new($stream)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
if ($XrPreview) {
    if ($Seconds -lt 15) { throw 'A visual preview needs at least 15 seconds for bounded session closure.' }
    $XrSessionProbe = $true
}
if ($PreviewReadbackTest -and ($XrPreview -or $XrSessionProbe -or $XrNegotiation)) { throw 'Readback-only validation must not open XR.' }
if ($PreviewReadbackTest) { $ExperimentalXrRenderer = $true }
if ($XrSessionProbe) { $XrNegotiation = $true }
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
Remove-Item Env:HOOKSHOT_VR_PREVIEW_CAPTURE_DIR -ErrorAction SilentlyContinue
if($PreviewCaptureDirectory){
    $PreviewCaptureDirectory=[IO.Path]::GetFullPath($PreviewCaptureDirectory)
    $workParent=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work')).TrimEnd('\','/')
    if(![string]::Equals((Split-Path -Parent $PreviewCaptureDirectory).TrimEnd('\','/'),$workParent,[StringComparison]::OrdinalIgnoreCase)){throw 'Image captures require a fresh direct child of project/work.'}
    if(Test-Path -LiteralPath $PreviewCaptureDirectory){throw 'Image capture directory already exists; no files were overwritten.'}
    if((Get-Item -LiteralPath $workParent -Force).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Capture root must not be a link.'}
    New-Item -ItemType Directory -Path $PreviewCaptureDirectory | Out-Null
    $env:HOOKSHOT_VR_PREVIEW_CAPTURE_DIR=$PreviewCaptureDirectory
}
$InstallationRoot = (Resolve-Path -LiteralPath $InstallationRoot).Path
$buildRoot = Join-Path $projectRoot 'work/game-build/sa/build'
$runRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot 'work/game-run'))
if ($runRoot.StartsWith($InstallationRoot.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar,
                       [StringComparison]::OrdinalIgnoreCase)) { throw 'Experimental runtime overlaps the installed game.' }
foreach ($candidate in @((Join-Path $projectRoot 'work'), $runRoot, (Join-Path $runRoot 'bin'),
                         (Join-Path $runRoot 'user'), (Join-Path $runRoot 'shots'))) {
    if ((Test-Path -LiteralPath $candidate) -and
        ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Runtime directory must not be a link: $candidate"
    }
}
$xrRenderer = Join-Path $projectRoot 'work/renderer-build/sa/build/hookshot_vk.dll'
if (($ExperimentalXrRenderer -or $XrNegotiation) -and !(Test-Path -LiteralPath $xrRenderer -PathType Leaf)) {
    throw 'Build the experimental renderer before requesting it.'
}
if ($XrNegotiation) {
    if (!$OpenXRLoader) { $OpenXRLoader = Join-Path $projectRoot 'work/vr-loader/openxr-sdk/src/loader/libopenxr_loader.dll' }
    $OpenXRLoader = (Resolve-Path -LiteralPath $OpenXRLoader).Path
}
. (Join-Path $PSScriptRoot 'compatibility.ps1')
$null=Assert-VrSupportedInstallation $projectRoot $InstallationRoot
$python=Get-VrPython ''
$fingerprint=Get-VrSourceFingerprint $projectRoot $python
Assert-VrBuildReceipt $projectRoot (Join-Path $projectRoot 'work/game-build/build-info.json') (Join-Path $buildRoot 'hookshot_vk.dll') (Join-Path $buildRoot 'hookshot.exe') $fingerprint $false
if($ExperimentalXrRenderer -or $XrNegotiation) {
    Assert-VrBuildReceipt $projectRoot (Join-Path $projectRoot 'work/renderer-build/build-info.json') $xrRenderer '' $fingerprint ([bool]($XrNegotiation -or $PreviewReadbackTest))
}
$image = Join-Path $projectRoot 'work/game-build/sa/image/image.bin'
foreach ($inputFile in @((Join-Path $buildRoot 'hookshot.exe'), (Join-Path $buildRoot 'hookshot_vk.dll'),
                         $image, (Join-Path $InstallationRoot 'hookshot_shaders.bin'))) {
    if (!(Test-Path -LiteralPath $inputFile -PathType Leaf)) { throw "Required local build input is missing: $inputFile" }
}
if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($runRoot)).AvailableFreeSpace -lt 8GB) {
    throw 'Less than 8 GiB free; runtime preparation not started.'
}
New-Item -ItemType Directory -Path $runRoot, (Join-Path $runRoot 'bin'), (Join-Path $runRoot 'user'),
    (Join-Path $runRoot 'shots'), (Join-Path $runRoot 'mods'), (Join-Path $runRoot 'overlay') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $buildRoot 'hookshot.exe') -Destination (Join-Path $runRoot 'bin/hookshot.exe')
Get-ChildItem -LiteralPath $buildRoot -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $runRoot 'bin')
}
if ($ExperimentalXrRenderer -or $XrNegotiation) {
    Copy-Item -LiteralPath $xrRenderer -Destination (Join-Path $runRoot 'bin/hookshot_vk.dll')
}
# A source checkout does not contain the Builder bundle's prebuilt SDL dependencies.
foreach ($name in @('SDL2.dll', 'SDL3.dll')) {
    $target = Join-Path $runRoot ('bin/' + $name)
    if (!(Test-Path -LiteralPath $target)) {
        $source = Join-Path $InstallationRoot ('bin/' + $name)
        if (!(Test-Path -LiteralPath $source)) { throw "Required SDL dependency is missing: $name" }
        Copy-Item -LiteralPath $source -Destination $target
    }
}
Copy-Item -LiteralPath $image -Destination (Join-Path $runRoot 'image.bin')
foreach ($name in @('hookshot_shaders.bin', 'hookshot_computes.bin', 'language.txt')) {
    $source = Join-Path $InstallationRoot $name
    $target = Join-Path $runRoot $name
    if ((Test-Path -LiteralPath $source -PathType Leaf) -and !(Test-Path -LiteralPath $target)) {
        Copy-Item -LiteralPath $source -Destination $target
    }
}
# Stable saves are read only when explicitly requested; each snapshot is a fresh private copy.
$userRoot = Join-Path $runRoot 'user'
if ($SnapshotStableSaves) {
    $sourceSaves = Join-Path $InstallationRoot 'user/save'
    if (!(Test-Path -LiteralPath $sourceSaves -PathType Container)) { throw 'Stable save directory is missing.' }
    $sourceEntries = @(Get-Item -LiteralPath $sourceSaves -Force) + @(Get-ChildItem -LiteralPath $sourceSaves -Recurse -Force)
    if (@($sourceEntries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) {
        throw 'Save snapshot source must not contain links.'
    }
    $userRoot = Join-Path $runRoot ('user-snapshot-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $userRoot | Out-Null
    Copy-Item -LiteralPath $sourceSaves -Destination $userRoot -Recurse
    foreach ($original in @($sourceEntries | Where-Object { !$_.PSIsContainer })) {
        $relative = $original.FullName.Substring($sourceSaves.Length).TrimStart('\','/')
        $copy = Join-Path (Join-Path $userRoot 'save') $relative
        if ((Get-VrFileHash $original.FullName) -ne (Get-VrFileHash $copy)) {
            throw 'Save snapshot verification failed; game not launched.'
        }
    }
}
# No native shader-program caches, stable settings or launcher are copied.
$env:HOS_INPUT_LIVE = Join-Path $runRoot 'input-live.txt'
[IO.File]::WriteAllText($env:HOS_INPUT_LIVE, '')
if ($LoadTestSave) { [IO.File]::WriteAllText($env:HOS_INPUT_LIVE, "20:A/150,30:A/150,40:A/150,60:A/150`n") }
$env:HOS_INPUT_SCRIPT_BACKGROUND = '1'
Remove-Item Env:HOS_INPUT_SCRIPT -ErrorAction SilentlyContinue
Remove-Item Env:HOS_INPUT_FRAME_CLOCK -ErrorAction SilentlyContinue
if ($SceneCapture) { $env:RX_VK_PARALLEL = '0'; $env:RX_VK_RECORDER = 'off' }
if ($EyeImageProbe) { $env:HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE = Join-Path $runRoot ('eye-image-probe-' + [Guid]::NewGuid().ToString('N') + '.json') }
else { Remove-Item Env:HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE -ErrorAction SilentlyContinue }
if($DepthSceneCapture){
    $env:RX_VK_XR_DEPTH_CAPTURE='1'
    $env:RX_VK_XR_DEPTH_STREAM=$(if($DepthSceneStream){'1'}else{'0'})
    $env:HOOKSHOT_VR_DEPTH_INTERVAL_MS=$DepthIntervalMs.ToString()
    $env:HOOKSHOT_VR_DEPTH_MAX_FRAMES=$DepthMaximumFrames.ToString()
    $env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE=Join-Path $runRoot ('depth-scene-'+[Guid]::NewGuid().ToString('N')+'.hsvrd')
    if($DepthCapturePath){
        $DepthCapturePath=[IO.Path]::GetFullPath($DepthCapturePath)
        $allowedRoot=[IO.Path]::GetFullPath((Join-Path $projectRoot 'work'))+[IO.Path]::DirectorySeparatorChar
        if(!$DepthCapturePath.StartsWith($allowedRoot,[StringComparison]::OrdinalIgnoreCase)){throw 'Depth stream output must stay inside project/work.'}
        if(!(Test-Path -LiteralPath (Split-Path -Parent $DepthCapturePath) -PathType Container)){throw 'Depth stream output directory must exist.'}
        foreach($suffix in @('','.json','.tmp','.armed')){if(Test-Path -LiteralPath ($DepthCapturePath+$suffix)){throw 'Depth capture requires a fresh output path; existing data was not modified.'}}
        $env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE=$DepthCapturePath
    }
    $env:HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE=$env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE+'.armed'
} else {
    $env:RX_VK_XR_DEPTH_CAPTURE='0';$env:RX_VK_XR_DEPTH_STREAM='0'
    Remove-Item Env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE -ErrorAction SilentlyContinue
    Remove-Item Env:HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE -ErrorAction SilentlyContinue
}
[string[]]$shiftSelectionArgs=if($TranslatedColorDrawProbe){@('--view-color-selection')}elseif($TranslatedDrawProbe){@('--view-shift-selection')}elseif($ShiftedDrawProbe){@('--lens-shift-selection')}else{@()}
$env:RX_VK_XR_ROOT_ALIAS_PROBE=if($GpuRootAliasProbe){'1'}else{'0'}
$env:RX_VK_XR_VIEW_COLOR_PROBE=if($TranslatedColorDrawProbe){'1'}else{'0'}
$env:RX_VK_XR_VIEW_SHIFT_PROBE=if($TranslatedDrawProbe){'1'}else{'0'}
$env:RX_VK_XR_VIEW_SHIFT_UNITS=$ViewShiftUnits.ToString([Globalization.CultureInfo]::InvariantCulture)
$env:RX_VK_XR_SHIFTED_DRAW_PROBE=if($ShiftedDrawProbe){'1'}else{'0'}
$env:RX_VK_XR_MATRIX_PATCH_PROBE=if($GpuEyeMatrixProbe){'1'}else{'0'}
if($IsolatedDrawProbe){
    foreach($name in @('RX_VK_DRAW_STATS','RX_VK_PASS_TIMING','RX_VK_PROBE','RX_VK_DUMP')){Remove-Item ('Env:'+$name) -ErrorAction SilentlyContinue}
    $env:RX_VK_XR_BASELINE_DRAW='1'
    $env:HOOKSHOT_VR_BASELINE_NONCE=[Guid]::NewGuid().ToString('N')
    $env:HOOKSHOT_VR_BASELINE_SOURCE=$fingerprint
    $env:HOOKSHOT_VR_BASELINE_SELECTION_FILE=Join-Path $runRoot ('baseline-selection-'+$env:HOOKSHOT_VR_BASELINE_NONCE+'.txt')
    $env:HOOKSHOT_VR_BASELINE_DRAW_FILE=Join-Path $runRoot ('baseline-draw-'+$env:HOOKSHOT_VR_BASELINE_NONCE+'.json')
} else {
    $env:RX_VK_XR_BASELINE_DRAW='0'
    foreach($name in @('HOOKSHOT_VR_BASELINE_NONCE','HOOKSHOT_VR_BASELINE_SOURCE','HOOKSHOT_VR_BASELINE_SELECTION_FILE','HOOKSHOT_VR_BASELINE_DRAW_FILE')){Remove-Item ('Env:'+$name) -ErrorAction SilentlyContinue}
}
if ($BaselineDrawPreflight) {
    $env:RX_VK_XR_BASELINE_PREFLIGHT='1'
    $env:HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE=Join-Path $runRoot ('baseline-preflight-'+[Guid]::NewGuid().ToString('N')+'.jsonl')
} else {
    $env:RX_VK_XR_BASELINE_PREFLIGHT='0'
    Remove-Item Env:HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE -ErrorAction SilentlyContinue
}
if ($EyeUniformProbe) { $env:HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE = Join-Path $runRoot ('eye-uniform-probe-' + [Guid]::NewGuid().ToString('N') + '.json') }
else { Remove-Item Env:HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE -ErrorAction SilentlyContinue }
if ($ComputeShaderUniformAudit) { $env:HOOKSHOT_VR_SHADER_UNIFORM_COMPUTE_ONLY = '1' }
else { Remove-Item Env:HOOKSHOT_VR_SHADER_UNIFORM_COMPUTE_ONLY -ErrorAction SilentlyContinue }
if ($ShaderUniformAudit) {
    $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR = Join-Path $runRoot ('shader-uniform-audit-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR | Out-Null
} else { Remove-Item Env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR -ErrorAction SilentlyContinue }
if ($CameraUniformAudit) {
    $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE = Join-Path $runRoot 'camera-uniform-blocks.jsonl'
    Remove-Item -LiteralPath $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ($env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE + '.armed') -ErrorAction SilentlyContinue
} else { Remove-Item Env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE -ErrorAction SilentlyContinue }
$env:HOOKSHOT_VR_IMAGE_LINEAGE = $(if ($ImageLineage) { '1' } else { '0' })
if ($ImageLineage) { $env:HOOKSHOT_VR_LINEAGE_FILE = Join-Path $runRoot 'image-lineage.json'; Remove-Item -LiteralPath $env:HOOKSHOT_VR_LINEAGE_FILE -ErrorAction SilentlyContinue } else { Remove-Item Env:HOOKSHOT_VR_LINEAGE_FILE -ErrorAction SilentlyContinue }
$env:HOOKSHOT_VR_SCENE_CAPTURE = $(if ($SceneCapture) { '1' } else { '0' })
$env:HOOKSHOT_VR_CAMERA_CAPTURE = $(if ($CameraCapture) { '1' } else { '0' })
$env:RX_VK_XR_WIDE_FOV = $(if($WideFov){'1'}else{'0'})
$env:RX_VK_XR_TEMPORAL_STEREO = $(if($TemporalStereo){'1'}else{'0'})
$env:RX_VK_XR_IMMERSIVE = $(if($ImmersivePreview -or $ImmersiveReadbackTest){'1'}else{'0'})
$env:HOOKSHOT_VR_IMMERSIVE_TEST = $(if($ImmersiveReadbackTest){'1'}else{'0'})
$env:HOOKSHOT_VR_IMMERSIVE_REPORT_FILE = Join-Path $runRoot 'immersive-result.json'
if($ImmersivePreview -or $ImmersiveReadbackTest){Remove-Item -LiteralPath $env:HOOKSHOT_VR_IMMERSIVE_REPORT_FILE -ErrorAction SilentlyContinue}
$env:RX_VK_XR_PREVIEW = $(if ($XrPreview) { '1' } else { '0' })
$env:HOOKSHOT_VR_PREVIEW_READBACK_TEST = $(if ($PreviewReadbackTest) { '1' } else { '0' })
if ($XrPreview) {
    $env:RX_VK_XR_DURATION_MS = ([Math]::Min($Seconds - 5, 180) * 1000).ToString()
    $env:RX_VK_RECORDER = 'off'
} else { Remove-Item Env:RX_VK_XR_DURATION_MS -ErrorAction SilentlyContinue }
$env:RX_VK_XR_SESSION_PROBE = $(if ($XrSessionProbe) { '1' } else { '0' })
$env:RX_VK_XR = $(if ($XrNegotiation) { '1' } else { '0' })
if ($XrNegotiation) { $env:HOOKSHOT_OPENXR_LOADER = $OpenXRLoader } else { Remove-Item Env:HOOKSHOT_OPENXR_LOADER -ErrorAction SilentlyContinue }
$env:RX_BACKEND = 'vulkan'
$env:HOS_HEADLESS = $(if ($Window) { '0' } else { '1' })
$env:HOS_WINDOW_NOACTIVATE = '1'
$env:HOS_FULLSCREEN = '0'
$env:HOS_AUDIO = $(if ($Audio) { '1' } else { '0' })
if ($Audio) {
    Remove-Item Env:HOS_AUDIO_MUTE -ErrorAction SilentlyContinue
    $env:HOS_AUDIO_CHANNELS = '2'
}
$env:HOS_DISCORD = '0'
$env:HOS_OUTPUT_RES = if($previewHD){'1920x1080'}else{'1280x720'}
if($previewHD){$env:HOS_DYNRES='0'}else{Remove-Item Env:HOS_DYNRES -ErrorAction SilentlyContinue}
$env:HOS_HANDHELD = if($previewHD){'0'}else{'1'}
$env:RX_VK_FRAMEGEN = '0'
$env:RX_VK_PRECOMPILE = $(if ($PrepareAllShaders) { '1' } else { '0' })
$env:HOOKSHOT_NO_UPDATE_CHECK = '1'
$env:HOS_MODS = Join-Path $runRoot 'mods'
$env:HOS_FS_OVERLAY = Join-Path $runRoot 'overlay'
$env:HOS_ENV_FILE = Join-Path $runRoot 'test_env.txt'
[IO.File]::WriteAllText($env:HOS_ENV_FILE, '# Isolated flat baseline; process environment controls this run.')
$options = Join-Path $runRoot 'hookshot_options.ini'
if (!(Test-Path -LiteralPath $options)) {
    [IO.File]::WriteAllText($options, "options_version=4`nquality_preset=1`nfullscreen=0`nhdr=0`nmicrophone=0`n")
}
Remove-Item Env:HOS_CONTROL -ErrorAction SilentlyContinue
$env:RX_VK_SHOTS = Join-Path $runRoot 'shots'
$env:RX_VK_SHOT_EVERY = '120'
if ($XrPreview -or $PreviewReadbackTest) {
    Remove-Item Env:RX_VK_SHOTS -ErrorAction SilentlyContinue
    $env:RX_VK_SHOT_W = if($previewHD){'1920'}else{'1280'}; $env:RX_VK_SHOT_H = if($previewHD){'1080'}else{'720'}
}
$exe = Join-Path $runRoot 'bin/hookshot.exe'
$argsForGame = @(('"' + (Join-Path $runRoot 'image.bin') + '"'), '--romfs',
                 ('"' + (Join-Path $InstallationRoot 'romfs') + '"'), '--user',
                 ('"' + $userRoot + '"'))
$p = Start-Process -FilePath $exe -ArgumentList $argsForGame -WorkingDirectory $runRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $runRoot 'stdout.txt') -RedirectStandardError (Join-Path $runRoot 'stderr.txt')
try { $p.PriorityClass = 'BelowNormal' } catch { }
$reason = 'time_limit'
$peakMB = 0
$xrNegotiated = $false
try {
    if($StopSignal){[ordered]@{native_pid=$p.Id;executable=$exe;started_utc=$p.StartTime.ToUniversalTime().ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath ($StopSignal+'.process.json') -Encoding UTF8}
    for ($elapsed = 0; $elapsed -lt $Seconds; $elapsed += 2) {
        if ($p.WaitForExit(2000)) { $reason = 'process_exited'; break }
        if($StopSignal -and (Test-Path -LiteralPath $StopSignal)){$reason='stop_requested';break}
        if ($XrNegotiation -and !$xrNegotiated) {
            $log = Read-ExperimentalLog
            $xrNegotiated = $log.Contains('xr: renderer Vulkan device negotiated')
            if (!$xrNegotiated -and ($log.Contains('rx: the Vulkan back end did not start') -or $log.Contains('rx: renderer: WebGPU'))) {
                $reason = 'xr_negotiation_failed'; break
            }
        }
        if ($XrSessionProbe) {
            $liveLog = Read-ExperimentalLog
            if ($liveLog -match 'xr: .*failed|xr: session owner exception|xr: session startup timed out|xr: renderer session probe could not start|xr: preview requires|xr: invalid session duration') {
                $reason = 'xr_session_failed'; break
            }
        }
        if($IsolatedDrawProbe -and $elapsed%10 -eq 0 -and (Test-Path -LiteralPath (Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'program-bindings.jsonl'))){
            # Atomic, nonce-scoped selection from this process's newly created capture directory.
            try {
            & $python -I -B (Join-Path $PSScriptRoot 'audit_graphics_programs.py') $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR --grammar $SpirvGrammar --output (Join-Path $runRoot 'baseline-live-programs.json') --baseline-selection $env:HOOKSHOT_VR_BASELINE_SELECTION_FILE --nonce $env:HOOKSHOT_VR_BASELINE_NONCE --source-fingerprint $fingerprint @shiftSelectionArgs 2> (Join-Path $runRoot 'baseline-selection-errors.txt')
            } catch { } # Expected while capture records are incomplete; no selection means no private draw.
            # No candidate yet is normal while the save loads; the native reader remains inactive.
        }
        if($DepthSceneCapture -and $elapsed -ge 80 -and !(Test-Path -LiteralPath $env:HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE)){[IO.File]::WriteAllText($env:HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE,'capture once')}
        $p.Refresh()
        $mb = [int]($p.WorkingSet64 / 1MB)
        $peakMB = [math]::Max($peakMB, $mb)
        if ($mb -gt ($MaximumWorkingSetGiB * 1024)) { $reason = 'memory_limit'; break }
        if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($runRoot)).AvailableFreeSpace -lt 4GB) { $reason = 'disk_limit'; break }
    }
} finally {
    if (!$p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
}
if ($XrNegotiation -and !$xrNegotiated) {
    $log = Read-ExperimentalLog
    $xrNegotiated = $log.Contains('xr: renderer Vulkan device negotiated')
    if (!$xrNegotiated -and $reason -eq 'time_limit') { $reason = 'xr_negotiation_unverified' }
}
$sessionApiErrors = $false
$sessionClosed = $false
$sessionValidPoses = $false
if ($XrSessionProbe) {
    $log = Read-ExperimentalLog
    $sessionApiErrors = $log -match 'xr: .*failed|xr: session owner exception|xr: session startup timed out|xr: renderer session probe could not start'
    $sessionClosed = $log.Contains('xr: renderer session probe closed;')
    $sessionValidPoses = $log -match 'xr: renderer session probe closed; frames [1-9][0-9]*, valid head [1-9][0-9]*, valid stereo poses [1-9][0-9]*'
    if ((!$sessionClosed -or !$sessionValidPoses -or $sessionApiErrors) -and $reason -eq 'time_limit') { $reason = 'xr_session_unverified' }
}
$previewSubmitted = $false
$readbackObserved = $false
if ($XrPreview -or $PreviewReadbackTest) {
    $previewLog = Read-ExperimentalLog
    $previewSubmitted = $previewLog -match 'xr: (flat quad preview|immersive mono projection) closed; quad frames [1-9][0-9]*, game images [1-9][0-9]*'
    $readbackObserved = $previewLog -match 'xr-preview: readback frames [1-9][0-9]*'
    if ($reason -eq 'time_limit' -and (($XrPreview -and !$previewSubmitted) -or ($PreviewReadbackTest -and !$readbackObserved))) {
        $reason = 'preview_unverified'
    }
}
$sceneObserved = $false
$submissionObserved = $false
if ($SceneCapture) {
    $sceneObserved = (Read-ExperimentalLog) -match 'vr-scene: .* matched [1-9][0-9]*, matched replay frames [1-9][0-9]*'
    $submissionObserved = (Read-ExperimentalLog) -match 'vr-submit-camera: candidates [1-9][0-9]*, matched [1-9][0-9]*,'
    if (!$submissionObserved -and $reason -eq 'time_limit') { $reason = 'scene_submission_unverified' }
}
$lineageVerified = $false
if ($ImageLineage -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_LINEAGE_FILE -PathType Leaf)) {
    $summaryPath = Join-Path $runRoot 'image-lineage-summary.json'
    & $python -I -B (Join-Path $PSScriptRoot 'analyze_lineage.py') $env:HOOKSHOT_VR_LINEAGE_FILE > $summaryPath
    if ($LASTEXITCODE -eq 0) {
        $summary = Get-Content -LiteralPath $summaryPath -Raw | ConvertFrom-Json
        $lineageVerified = $summary.camera_seed_events_in_slice -gt 0 -and $summary.camera_dependent_events_in_slice -gt 0 -and !$summary.replay_ready
    }
}
if ($ImageLineage -and !$lineageVerified -and $reason -eq 'time_limit') { $reason = 'image_lineage_unverified' }
$uniformAuditCaptured = $false
$uniformAuditMultipleViews = $false
if ($CameraUniformAudit -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE -PathType Leaf)) {
    $auditPath = Join-Path $runRoot 'camera-uniform-audit.json'
    & $python -I -B (Join-Path $PSScriptRoot 'audit_camera_uniforms.py') $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE > $auditPath
    if ($LASTEXITCODE -eq 0) {
        $audit = Get-Content -LiteralPath $auditPath -Raw | ConvertFrom-Json
        $uniformAuditCaptured = $audit.samples -gt 0 -and !$audit.safe_to_patch
        $uniformAuditMultipleViews = [bool]$audit.multiple_view_confirmation
    }
}
if ($CameraUniformAudit -and (!$uniformAuditCaptured -or !$uniformAuditMultipleViews) -and $reason -eq 'time_limit') { $reason = 'camera_uniform_audit_unverified' }
$shaderAuditObserved = $false
if ($ShaderUniformAudit -and (Test-Path -LiteralPath (Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'program-bindings.jsonl'))) {
    $shaderSummary = Join-Path $runRoot 'shader-uniform-audit-summary.json'
    & $python -I -B (Join-Path $PSScriptRoot 'audit_shader_bindings.py') $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR > $shaderSummary
    if ($LASTEXITCODE -eq 0) {
        $audit = Get-Content -LiteralPath $shaderSummary -Raw | ConvertFrom-Json
        $shaderAuditObserved = $audit.primary_camera_bindings -gt 0 -and $audit.queries_with_static_reads -gt 0 -and $audit.missing_primary_modules -eq 0 -and !$audit.safe_to_patch
        if ($ComputeShaderUniformAudit) { $shaderAuditObserved = $audit.observed_programs -gt 0 -and $audit.observed_bindings -gt 0 -and @($audit.observed_nvn_stages).Count -eq 1 -and $audit.observed_nvn_stages[0] -eq 5 -and $audit.missing_observed_modules -eq 0 -and !$audit.safe_to_patch }
    }
}
if ($ImageLineage -and $lineageVerified -and $ShaderUniformAudit -and $shaderAuditObserved) {
    $associatedGraph = Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'image-lineage.json'
    Copy-Item -LiteralPath (Join-Path $runRoot 'image-lineage.json') -Destination $associatedGraph
    $baselinePreflightHash=$null
    if($BaselineDrawPreflight -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE)) {
        $associatedPreflight=Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'baseline-preflight.jsonl'
        Copy-Item -LiteralPath $env:HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE -Destination $associatedPreflight
        $baselinePreflightHash=Get-VrFileHash $associatedPreflight
    }
    [ordered]@{schema=1;producer='vr/run-game.ps1';same_process_capture=$true;baseline_preflight_sha256=$baselinePreflightHash;
        lineage_sha256=(Get-VrFileHash $associatedGraph);
        bindings_sha256=(Get-VrFileHash (Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'program-bindings.jsonl'));
        source_fingerprint=$fingerprint;scope='first bindings and representative frame from one bounded process; not per-dispatch proof'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR 'capture-association.json') -Encoding UTF8
}
if ($ShaderUniformAudit -and !$shaderAuditObserved -and $reason -eq 'time_limit') { $reason = 'shader_uniform_audit_unverified' }
$eyeImageProbeComplete=$false
if($EyeImageProbe -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE)) {
    $eyeProbe=Get-Content -LiteralPath $env:HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE -Raw | ConvertFrom-Json
    $eyeImageProbeComplete=$eyeProbe.schema -eq 1 -and $eyeProbe.copy_recorded -and $eyeProbe.gpu_completion_verified -and $eyeProbe.resources_released -and $eyeProbe.view_objects_created -eq 4 -and $eyeProbe.view_objects_released -and !$eyeProbe.source_objects_replaced -and !$eyeProbe.stereo_replay_enabled
}
if($EyeImageProbe -and !$eyeImageProbeComplete -and $reason -eq 'time_limit'){$reason='eye_image_probe_unverified'}
$eyeUniformProbeComplete=$false
if($EyeUniformProbe -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE)) {
    $uniformProbe=Get-Content -LiteralPath $env:HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE -Raw | ConvertFrom-Json
    $eyeUniformProbeComplete=$uniformProbe.schema -eq 1 -and $uniformProbe.copy_recorded -and $uniformProbe.gpu_completion_verified -and $uniformProbe.full_payloads_validated -and $uniformProbe.resources_released -and !$uniformProbe.source_objects_replaced -and !$uniformProbe.stereo_replay_enabled
}
if($EyeUniformProbe -and !$eyeUniformProbeComplete -and $reason -eq 'time_limit'){$reason='eye_uniform_probe_unverified'}
$gpuMatrixVerified=$false
$isolatedDrawMatched=$false
if($IsolatedDrawProbe -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_BASELINE_DRAW_FILE)){
    $baselineDraw=Get-Content -LiteralPath $env:HOOKSHOT_VR_BASELINE_DRAW_FILE -Raw | ConvertFrom-Json
    $isolatedDrawMatched=$baselineDraw.schema -eq 1 -and ($baselineDraw.status -eq $(if($TranslatedColorDrawProbe){'view_color_matched'}elseif($TranslatedDrawProbe){'view_shift_matched'}elseif($ShiftedDrawProbe){'lens_shift_matched'}else{'baseline_matched'})) -and $baselineDraw.resources_released -and $baselineDraw.changed_original_pixels -gt 0 -and $baselineDraw.left_mismatched_pixels -eq 0 -and $baselineDraw.right_mismatched_pixels -eq 0 -and $baselineDraw.unchanged_camera_baseline -and !$baselineDraw.full_scene_stereo -and !$baselineDraw.headset_presentation
}
if($GpuEyeMatrixProbe){$gpuMatrixVerified=(!$TranslatedColorDrawProbe -or $baselineDraw.view_color_requested) -and (!$TranslatedDrawProbe -or $baselineDraw.view_shift_requested) -and $isolatedDrawMatched -and $baselineDraw.matrix_probe_requested -and $baselineDraw.matrix_probe_validated -and $baselineDraw.matrix_source_unchanged -and ($baselineDraw.patched_buffers_bound_to_draw -eq [bool]$ShiftedDrawProbe) -and (!$ShiftedDrawProbe -or $baselineDraw.shifted_draw_verified);if(!$gpuMatrixVerified){$isolatedDrawMatched=$false}}
$rootAliasVerified=$GpuRootAliasProbe -and $gpuMatrixVerified -and $baselineDraw.root_alias_requested -and $baselineDraw.root_alias_verified -and $baselineDraw.root_indirect_verified -and $baselineDraw.root_indirect_draws_recorded
if($GpuRootAliasProbe -and !$rootAliasVerified){$isolatedDrawMatched=$false}
if($IsolatedDrawProbe -and !$isolatedDrawMatched -and $reason -eq 'time_limit'){$reason='isolated_draw_unverified'}
$depthSceneComplete=$false
if($DepthSceneCapture -and (Test-Path -LiteralPath ($env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE+'.json')) -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE)){
    $depthStatus=Get-Content -LiteralPath ($env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE+'.json') -Raw | ConvertFrom-Json
    $depthSceneComplete=$depthStatus.schema -eq 1 -and (($DepthSceneStream -and $depthStatus.completed_frames -ge 2 -and $depthStatus.status -in @('streaming','recorded','stream_complete','publication_retry')) -or (!$DepthSceneStream -and $depthStatus.status -eq 'completed' -and $depthStatus.gpu_complete)) -and $depthStatus.color_view_format -in @(122,123) -and $depthStatus.width -gt 0 -and $depthStatus.height -gt 0 -and (Get-Item -LiteralPath $env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE).Length -eq (132+8*$depthStatus.width*$depthStatus.height)
}
if($DepthSceneCapture -and !$depthSceneComplete -and $reason -eq 'time_limit'){$reason='depth_scene_capture_unverified'}
$immersiveVerified=$false
$temporalPairObserved=$false
if($TemporalStereo -and $ImmersiveReadbackTest){$temporalPairObserved=(Read-ExperimentalLog) -match 'xr-preview: temporal stereo pairs [1-9][0-9]*, distinct images 1'}
if($TemporalStereo -and $ImmersivePreview){$temporalPairObserved=(Read-ExperimentalLog) -match 'xr: temporal stereo projection closed; frames [1-9][0-9]*'}
$immersiveReport=$null
if(($ImmersivePreview -or $ImmersiveReadbackTest) -and (Test-Path -LiteralPath $env:HOOKSHOT_VR_IMMERSIVE_REPORT_FILE)){
    $immersiveReport=Get-Content -LiteralPath $env:HOOKSHOT_VR_IMMERSIVE_REPORT_FILE -Raw | ConvertFrom-Json
    $immersiveVerified=$immersiveReport.schema -eq 1 -and $immersiveReport.camera_overrides -gt 0 -and $immersiveReport.matched_draws -gt 0 -and $immersiveReport.published_images -gt 0 -and ([bool]$immersiveReport.monoscopic -eq !$TemporalStereo) -and (!$TemporalStereo -or ($temporalPairObserved -and $immersiveReport.temporal_stereo -and $immersiveReport.left_images -gt 0 -and $immersiveReport.right_images -gt 0)) -and (!$WideFov -or ($immersiveReport.lens_updates -gt 0 -and [math]::Abs($immersiveReport.vertical_fov_rad-$immersiveReport.requested_vertical_fov_rad) -lt 0.02)) -and !$immersiveReport.native_stereo -and ([bool]$immersiveReport.synthetic_pose -eq [bool]$ImmersiveReadbackTest)
}
if(($ImmersivePreview -or $ImmersiveReadbackTest) -and !$immersiveVerified -and $reason -eq 'time_limit'){$reason='immersive_camera_unverified'}
[ordered]@{ root_alias_probe_requested=[bool]$GpuRootAliasProbe;root_alias_probe_verified=[bool]$rootAliasVerified;translated_draw_requested=[bool]$TranslatedDrawProbe;low_latency_preview=[bool]$LowLatencyPreview;docked_preview=[bool]$previewHD;preview_resolution=$PreviewResolution;fov_margin_percent=$FovMarginPercent;native_render_resolution=[bool]$previewHD;shifted_draw_requested=[bool]$ShiftedDrawProbe;shifted_draw_verified=[bool]($ShiftedDrawProbe -and $gpuMatrixVerified);gpu_matrix_probe_requested=[bool]$GpuEyeMatrixProbe;gpu_matrix_probe_verified=[bool]$gpuMatrixVerified;temporal_pair_observed=$temporalPairObserved;immersive_requested=[bool]($ImmersivePreview -or $ImmersiveReadbackTest);immersive_camera_verified=$immersiveVerified;immersive_report=$immersiveReport;depth_scene_stream_requested=[bool]$DepthSceneStream;depth_scene_capture_completed=$depthSceneComplete;depth_scene_capture_requested=[bool]$DepthSceneCapture;depth_scene_capture_file=$env:HOOKSHOT_VR_DEPTH_CAPTURE_FILE;isolated_draw_requested=[bool]$IsolatedDrawProbe;isolated_draw_matched=$isolatedDrawMatched;isolated_draw_file=$env:HOOKSHOT_VR_BASELINE_DRAW_FILE;baseline_draw_preflight_requested=[bool]$BaselineDrawPreflight;baseline_draw_preflight_file=$env:HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE;eye_uniform_probe_requested=[bool]$EyeUniformProbe;eye_uniform_probe_completed=$eyeUniformProbeComplete;eye_uniform_probe_file=$env:HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE;eye_image_probe_requested=[bool]$EyeImageProbe;eye_image_probe_completed=$eyeImageProbeComplete;eye_image_probe_file=$env:HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE; compute_shader_uniform_audit_requested = [bool]$ComputeShaderUniformAudit; shader_uniform_audit_requested = [bool]$ShaderUniformAudit; shader_uniform_audit_observed = $shaderAuditObserved; shader_uniform_audit_directory = $env:HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR; camera_uniform_audit_requested = [bool]$CameraUniformAudit; camera_uniform_audit_captured = $uniformAuditCaptured; camera_uniform_audit_multiple_views = $uniformAuditMultipleViews; image_lineage_requested = [bool]$ImageLineage; image_lineage_verified = $lineageVerified; audio_requested = [bool]$Audio; scene_capture_requested = [bool]$SceneCapture; scene_camera_matched = $sceneObserved; scene_submission_matched = $submissionObserved; flat_quad_preview_requested = [bool]($XrPreview -and !$ImmersivePreview); flat_quad_submitted = ($previewSubmitted -and !$ImmersivePreview); readback_test_requested = [bool]$PreviewReadbackTest; readback_observed = $readbackObserved; camera_capture_requested = [bool]$CameraCapture; xr_session_api_errors = $sessionApiErrors; xr_session_requested = [bool]$XrSessionProbe; xr_session_closed = $sessionClosed; xr_session_valid_poses = $sessionValidPoses; experimental_xr_renderer = [bool]($ExperimentalXrRenderer -or $XrNegotiation); xr_negotiation_requested = [bool]$XrNegotiation; xr_device_negotiated = $xrNegotiated; reason = $reason; exit_code = $p.ExitCode; peak_working_set_MB = $peakMB; runtime_root = $runRoot;
             user_root = $userRoot; save_snapshot = [bool]$SnapshotStableSaves; headless = !$Window; upfront_shader_precompile = [bool]$PrepareAllShaders; gameplay_verified = $false } | ConvertTo-Json
if ($reason -eq 'memory_limit' -or $reason -eq 'disk_limit') { exit 124 }
if ($reason -eq 'process_exited' -and $p.ExitCode -ne 0) { exit 1 }

if ($XrNegotiation -and !$xrNegotiated) { exit 1 }

if ($XrSessionProbe -and (!$sessionClosed -or !$sessionValidPoses -or $sessionApiErrors)) { exit 1 }

if (($XrPreview -and !$previewSubmitted) -or ($PreviewReadbackTest -and !$readbackObserved)) { exit 1 }

if ($SceneCapture -and !$submissionObserved) { exit 1 }

if ($ImageLineage -and !$lineageVerified) { exit 1 }

if ($CameraUniformAudit -and (!$uniformAuditCaptured -or !$uniformAuditMultipleViews)) { exit 1 }

if ($ShaderUniformAudit -and !$shaderAuditObserved) { exit 1 }

if($EyeImageProbe -and !$eyeImageProbeComplete){exit 1}

if($EyeUniformProbe -and !$eyeUniformProbeComplete){exit 1}

if($DepthSceneCapture -and !$depthSceneComplete){exit 1}

if(($ImmersivePreview -or $ImmersiveReadbackTest) -and !$immersiveVerified){exit 1}
