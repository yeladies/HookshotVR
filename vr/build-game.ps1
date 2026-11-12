param(
    [string]$InstallationRoot = (Join-Path $env:USERPROFILE 'Games/Hookshot'),
    [string]$BuilderToolsRoot,
    [ValidateRange(1, 4)][int]$Jobs = 2,
    [ValidateRange(8, 100)][int]$MinimumFreeGiB = 16,
    [switch]$PlanOnly,
    [switch]$SkipLift,
    [switch]$LiftOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$InstallationRoot = (Resolve-Path -LiteralPath $InstallationRoot).Path
$gameWork = [IO.Path]::GetFullPath((Join-Path $projectRoot 'work/game-build'))
if (!$gameWork.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    $gameWork.StartsWith($InstallationRoot.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build output must be inside this project and outside the installed game.'
}
# Do not let an existing junction redirect generated files into the stable game or another directory.
foreach ($candidate in @((Join-Path $projectRoot 'work'), $gameWork)) {
    if ((Test-Path -LiteralPath $candidate) -and
        ((Get-Item -LiteralPath $candidate -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Build directory must not be a link: $candidate"
    }
}
if (!$BuilderToolsRoot) {
    $versions = Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions'
    if (Test-Path -LiteralPath $versions) {
        $BuilderToolsRoot = Get-ChildItem -LiteralPath $versions -Directory |
            Sort-Object LastWriteTime -Descending | ForEach-Object {
                $candidate = Join-Path $_.FullName 'HookshotBuilder/tools'
                if (Test-Path -LiteralPath (Join-Path $candidate 'python/python.exe')) { $candidate }
            } | Select-Object -First 1
    }
}
if (!$BuilderToolsRoot) { throw 'Specify -BuilderToolsRoot for an existing Hookshot Builder tool bundle.' }
$BuilderToolsRoot = (Resolve-Path -LiteralPath $BuilderToolsRoot).Path
$py = Join-Path $BuilderToolsRoot 'python/python.exe'
$mingw = Join-Path $BuilderToolsRoot 'llvm-mingw'
$exefs = Join-Path $InstallationRoot '.hookshot_exefs'
$romfs = Join-Path $InstallationRoot 'romfs'
foreach ($file in @($py, (Join-Path $mingw 'bin/clang.exe'), (Join-Path $exefs 'main'),
                    (Join-Path $exefs 'rtld'), (Join-Path $exefs 'sdk'),
                    (Join-Path $BuilderToolsRoot 'work/hot_funcs.txt'))) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Required input is missing: $file" }
}
if (!(Test-Path -LiteralPath $romfs -PathType Container)) { throw 'Installed RomFS folder is missing.' }
. (Join-Path $PSScriptRoot 'compatibility.ps1')
$buildId=Assert-VrSupportedInstallation $projectRoot $InstallationRoot
$drive = [IO.DriveInfo]::new([IO.Path]::GetPathRoot($gameWork))
if ($drive.AvailableFreeSpace -lt ($MinimumFreeGiB * 1GB)) {
    throw "Less than $MinimumFreeGiB GiB free on the build drive. No build started."
}
& $py -I -B -c 'import lz4,numpy,capstone,unicorn,zstandard'
if ($LASTEXITCODE -ne 0) { throw 'The selected Builder Python lacks required packages.' }
[ordered]@{
    source = $projectRoot
    read_only_game_input = $InstallationRoot
    generated_files = $gameWork
    output = (Join-Path $gameWork 'sa/build')
    romfs_copied = $false
    game_launched = $false
    jobs = $Jobs
    free_GiB = [math]::Round($drive.AvailableFreeSpace / 1GB, 1)
} | ConvertTo-Json
if ($PlanOnly) { return }

$env:PYTHONDONTWRITEBYTECODE = '1'
$env:OOT_WORK = $gameWork
$env:OOT_EXEFS = $exefs
$env:OOT_ROMFS = $romfs
$env:HOOKSHOT_MINGW = $mingw
$env:RX_CPM = Join-Path $BuilderToolsRoot 'cpm'
$env:RX_RELEASE = '1'
$env:A64_HOT_CHUNK = '1'
$env:WGPU_NATIVE = Join-Path $BuilderToolsRoot 'wgpu-native'
$env:PATH = (Join-Path $mingw 'bin') + ';' + $env:PATH
Remove-Item Env:RX_VK_ONLY -ErrorAction SilentlyContinue
Remove-Item Env:RX_PGO -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path (Join-Path $gameWork 'image') -Force | Out-Null
foreach ($name in @('hot_funcs.txt', 'hot_order.txt', 'hookshot_pieces.prof', 'hookshot.res')) {
    $source = Join-Path $BuilderToolsRoot ('work/' + $name)
    $target = Join-Path $gameWork $name
    if (!$SkipLift -and (Test-Path -LiteralPath $source)) { Copy-Item -LiteralPath $source -Destination $target }
}
$env:A64_HOT_FUNCS = Join-Path $gameWork 'hot_funcs.txt'
$profile = Join-Path $gameWork 'hookshot_pieces.prof'
if (Test-Path -LiteralPath $profile) { $env:RX_PGO = $profile }
$chunks = Join-Path $BuilderToolsRoot 'work/image/chunks.txt'
if (!$SkipLift -and (Test-Path -LiteralPath $chunks)) {
    Copy-Item -LiteralPath $chunks -Destination (Join-Path $gameWork 'image/chunks.txt')
}
Push-Location -LiteralPath $projectRoot
try {
    if (!$SkipLift) {
        & $py -I -B -u nx2/lift.py --standalone --write --work $gameWork --exefs $exefs
        if ($LASTEXITCODE -ne 0) { throw 'Source translation failed; see the local build log.' }
        $hotWhole = Join-Path $gameWork 'sa/gen/code_hot.c'
        if (Test-Path -LiteralPath $hotWhole) {
            & $py -I -B -u nx2/split_hot.py (Join-Path $gameWork 'sa/gen')
            if ($LASTEXITCODE -ne 0) { throw 'Hot function partitioning failed.' }
        }
    } elseif (!(Test-Path -LiteralPath (Join-Path $gameWork 'sa/gen/hos_image.c')) -or
              !(Test-Path -LiteralPath $env:A64_HOT_FUNCS) -or
              !(Get-ChildItem -LiteralPath (Join-Path $gameWork 'sa/gen') -Filter 'code_hot_*.c' -ErrorAction SilentlyContinue)) {
        throw '-SkipLift requires an already completed translation from the current source.'
    }
    if ($LiftOnly) { Write-Output 'Translation completed; native compilation not requested.'; return }
    if ($drive.AvailableFreeSpace -lt (8GB)) { throw 'Less than 8 GiB remain after translation. Compile not started.' }
    $sourceBefore=Get-VrSourceFingerprint $projectRoot $py
    & $py -I -B -u nx2/build.py --standalone --jobs $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Native compilation failed; see the local build log.' }
    if((Get-VrSourceFingerprint $projectRoot $py) -ne $sourceBefore){throw 'Sources changed during compilation; no build receipt issued.'}
    Write-VrBuildReceipt $projectRoot (Join-Path $gameWork 'build-info.json') (Join-Path $gameWork 'sa/build/hookshot_vk.dll') (Join-Path $gameWork 'sa/build/hookshot.exe') $sourceBefore ([bool]$env:HOOKSHOT_OPENXR_SDK)
    Write-Output 'Experimental binaries built. No game was launched or installed.'
} finally { Pop-Location }
