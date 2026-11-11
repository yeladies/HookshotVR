param(
    [string]$ToolchainRoot,
    [string]$SdkRoot,
    [switch]$RunProbe
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot 'work/vr-build'
if (!$SdkRoot) { $SdkRoot = Join-Path $projectRoot 'work/OpenXR-SDK' }
$sdkCommit = '24a1f2b92d212501a2b1b8d30ffd53abf55fcfa8'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

if (!(Test-Path -LiteralPath (Join-Path $SdkRoot 'CMakeLists.txt'))) {
    if (Test-Path -LiteralPath $SdkRoot) { throw "SDK folder already exists but is incomplete: $SdkRoot" }
    Invoke-Checked 'git' @('clone', '--depth', '1', '--branch', 'release-1.1.65',
        'https://github.com/KhronosGroup/OpenXR-SDK.git', $SdkRoot)
}
$haveCommit = & git -C $SdkRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $haveCommit -ne $sdkCommit) { throw 'OpenXR SDK revision differs from the pinned revision.' }

if (!$ToolchainRoot -and !(Get-Command clang -ErrorAction SilentlyContinue)) {
    $versions = Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions'
    if (Test-Path -LiteralPath $versions) {
        $ToolchainRoot = Get-ChildItem -LiteralPath $versions -Directory |
            Sort-Object LastWriteTime -Descending | ForEach-Object {
                $candidate = Join-Path $_.FullName 'HookshotBuilder/tools/llvm-mingw'
                if (Test-Path -LiteralPath (Join-Path $candidate 'bin/clang.exe')) { $candidate }
            } | Select-Object -First 1
    }
}
$configure = @('-S', $PSScriptRoot, '-B', $buildRoot, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    "-DOPENXR_SDK_SOURCE_DIR=$SdkRoot")
if ($ToolchainRoot) {
    $cc = Join-Path $ToolchainRoot 'bin/clang.exe'
    $cxx = Join-Path $ToolchainRoot 'bin/clang++.exe'
    if (!(Test-Path -LiteralPath $cc) -or !(Test-Path -LiteralPath $cxx)) { throw 'llvm-mingw compilers not found.' }
    # CMake/Ninja need llvm-mingw's linker and resource compiler too. Process-local only.
    $env:PATH = (Join-Path $ToolchainRoot 'bin') + [IO.Path]::PathSeparator + $env:PATH
    $configure += "-DCMAKE_C_COMPILER=$cc", "-DCMAKE_CXX_COMPILER=$cxx"
}
Invoke-Checked 'cmake' $configure
Invoke-Checked 'cmake' @('--build', $buildRoot, '--parallel', '2')
Invoke-Checked 'ctest' @('--test-dir', $buildRoot, '--output-on-failure')
if ($RunProbe) {
    $probe = Join-Path $buildRoot 'hookshot_vr_probe.exe'
    & $probe
    exit $LASTEXITCODE
}
