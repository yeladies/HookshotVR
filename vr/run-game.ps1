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
    [switch]$PreviewReadbackTest,
    [switch]$CameraCapture,
    [switch]$SceneCapture,
    [switch]$ImageLineage,
    [switch]$CameraUniformAudit,
    [switch]$Audio,
    [string]$OpenXRLoader
)
$ErrorActionPreference = 'Stop'
if ($ImageLineage -or $CameraUniformAudit) { $SceneCapture = $true }
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
        if ((Get-FileHash -LiteralPath $original.FullName).Hash -ne (Get-FileHash -LiteralPath $copy).Hash) {
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
if ($CameraUniformAudit) {
    $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE = Join-Path $runRoot 'camera-uniform-blocks.jsonl'
    Remove-Item -LiteralPath $env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath ($env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE + '.armed') -ErrorAction SilentlyContinue
} else { Remove-Item Env:HOOKSHOT_VR_CAMERA_BLOCKS_FILE -ErrorAction SilentlyContinue }
$env:HOOKSHOT_VR_IMAGE_LINEAGE = $(if ($ImageLineage) { '1' } else { '0' })
if ($ImageLineage) { $env:HOOKSHOT_VR_LINEAGE_FILE = Join-Path $runRoot 'image-lineage.json'; Remove-Item -LiteralPath $env:HOOKSHOT_VR_LINEAGE_FILE -ErrorAction SilentlyContinue } else { Remove-Item Env:HOOKSHOT_VR_LINEAGE_FILE -ErrorAction SilentlyContinue }
$env:HOOKSHOT_VR_SCENE_CAPTURE = $(if ($SceneCapture) { '1' } else { '0' })
$env:HOOKSHOT_VR_CAMERA_CAPTURE = $(if ($CameraCapture) { '1' } else { '0' })
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
$env:HOS_OUTPUT_RES = '1280x720'
$env:HOS_HANDHELD = '1'
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
    $env:RX_VK_SHOT_W = '1280'; $env:RX_VK_SHOT_H = '720'
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
    for ($elapsed = 0; $elapsed -lt $Seconds; $elapsed += 2) {
        if ($p.WaitForExit(2000)) { $reason = 'process_exited'; break }
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
    $previewSubmitted = $previewLog -match 'xr: flat quad preview closed; quad frames [1-9][0-9]*, game images [1-9][0-9]*'
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
[ordered]@{ camera_uniform_audit_requested = [bool]$CameraUniformAudit; camera_uniform_audit_captured = $uniformAuditCaptured; camera_uniform_audit_multiple_views = $uniformAuditMultipleViews; image_lineage_requested = [bool]$ImageLineage; image_lineage_verified = $lineageVerified; audio_requested = [bool]$Audio; scene_capture_requested = [bool]$SceneCapture; scene_camera_matched = $sceneObserved; scene_submission_matched = $submissionObserved; flat_quad_preview_requested = [bool]$XrPreview; flat_quad_submitted = $previewSubmitted; readback_test_requested = [bool]$PreviewReadbackTest; readback_observed = $readbackObserved; camera_capture_requested = [bool]$CameraCapture; xr_session_api_errors = $sessionApiErrors; xr_session_requested = [bool]$XrSessionProbe; xr_session_closed = $sessionClosed; xr_session_valid_poses = $sessionValidPoses; experimental_xr_renderer = [bool]($ExperimentalXrRenderer -or $XrNegotiation); xr_negotiation_requested = [bool]$XrNegotiation; xr_device_negotiated = $xrNegotiated; reason = $reason; exit_code = $p.ExitCode; peak_working_set_MB = $peakMB; runtime_root = $runRoot;
             user_root = $userRoot; save_snapshot = [bool]$SnapshotStableSaves; headless = !$Window; upfront_shader_precompile = [bool]$PrepareAllShaders; gameplay_verified = $false } | ConvertTo-Json
if ($reason -eq 'memory_limit' -or $reason -eq 'disk_limit') { exit 124 }
if ($reason -eq 'process_exited' -and $p.ExitCode -ne 0) { exit 1 }

if ($XrNegotiation -and !$xrNegotiated) { exit 1 }

if ($XrSessionProbe -and (!$sessionClosed -or !$sessionValidPoses -or $sessionApiErrors)) { exit 1 }

if (($XrPreview -and !$previewSubmitted) -or ($PreviewReadbackTest -and !$readbackObserved)) { exit 1 }

if ($SceneCapture -and !$submissionObserved) { exit 1 }

if ($ImageLineage -and !$lineageVerified) { exit 1 }

if ($CameraUniformAudit -and (!$uniformAuditCaptured -or !$uniformAuditMultipleViews)) { exit 1 }
