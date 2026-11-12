param(
    [string]$BuilderToolsRoot,
    [string]$OpenXRSdkRoot,
    [ValidateRange(1,4)][int]$Jobs = 2,
    [switch]$PlanOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$workRoot = Join-Path $projectRoot 'work/renderer-build'
foreach ($candidate in @((Join-Path $projectRoot 'work'), $workRoot)) {
    if ((Test-Path -LiteralPath $candidate) -and
        ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Build directory must not be a link: $candidate"
    }
}
if (!$BuilderToolsRoot) {
    $versions = Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions'
    $BuilderToolsRoot = Get-ChildItem -LiteralPath $versions -Directory | Sort-Object LastWriteTime -Descending |
        ForEach-Object { $p = Join-Path $_.FullName 'HookshotBuilder/tools'; if (Test-Path -LiteralPath (Join-Path $p 'python/python.exe')) { $p } } |
        Select-Object -First 1
}
$BuilderToolsRoot = (Resolve-Path -LiteralPath $BuilderToolsRoot).Path
foreach ($relative in @('python/python.exe','llvm-mingw/bin/clang++.exe','cpm')) {
    if (!(Test-Path -LiteralPath (Join-Path $BuilderToolsRoot $relative))) { throw "Missing Builder dependency: $relative" }
}
if ($OpenXRSdkRoot) {
    $OpenXRSdkRoot = (Resolve-Path -LiteralPath $OpenXRSdkRoot).Path
    if (!(Test-Path -LiteralPath (Join-Path $OpenXRSdkRoot 'include/openxr/openxr_platform.h'))) { throw 'OpenXR SDK headers missing.' }
}
if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($workRoot)).AvailableFreeSpace -lt 4GB) { throw 'Less than 4 GiB free.' }
[ordered]@{source=$projectRoot; output=(Join-Path $workRoot 'sa/build'); openxr_sdk=$OpenXRSdkRoot; jobs=$Jobs; game_built=$false; game_launched=$false} | ConvertTo-Json
. (Join-Path $PSScriptRoot 'compatibility.ps1')
$null=Get-VrManifest $projectRoot
if ($PlanOnly) { return }
New-Item -ItemType Directory -Path $workRoot -Force | Out-Null
$env:OOT_WORK = $workRoot
$env:HOOKSHOT_MINGW = Join-Path $BuilderToolsRoot 'llvm-mingw'
$env:RX_CPM = Join-Path $BuilderToolsRoot 'cpm'
$env:RX_RELEASE = '1'
$env:RX_VK_ONLY = '1'
$env:PYTHONDONTWRITEBYTECODE = '1'
if ($OpenXRSdkRoot) { $env:HOOKSHOT_OPENXR_SDK = $OpenXRSdkRoot } else { Remove-Item Env:HOOKSHOT_OPENXR_SDK -ErrorAction SilentlyContinue }
Remove-Item Env:RX_PGO -ErrorAction SilentlyContinue
Remove-Item Env:HOOKSHOT_VK_PREBUILT -ErrorAction SilentlyContinue
$env:PATH = (Join-Path $BuilderToolsRoot 'llvm-mingw/bin') + ';' + $env:PATH
Push-Location -LiteralPath $projectRoot
try {
    $python=Get-VrPython $BuilderToolsRoot
    $sourceBefore=Get-VrSourceFingerprint $projectRoot $python
    & $python -I -B -u nx2/build.py --standalone --jobs $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Renderer compilation failed.' }
    if((Get-VrSourceFingerprint $projectRoot $python) -ne $sourceBefore){throw 'Sources changed during compilation; no build receipt issued.'}
    Write-VrBuildReceipt $projectRoot (Join-Path $workRoot 'build-info.json') (Join-Path $workRoot 'sa/build/hookshot_vk.dll') '' $sourceBefore ([bool]$OpenXRSdkRoot)
} finally { Pop-Location }