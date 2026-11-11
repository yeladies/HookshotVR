param(
    [string]$InstallationRoot = (Join-Path $env:USERPROFILE 'Games/Hookshot'),
    [ValidateRange(5, 300)][int]$Seconds = 60,
    [ValidateRange(4, 14)][int]$MaximumWorkingSetGiB = 11,
    [switch]$Window
)
$ErrorActionPreference = 'Stop'
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
# No stable saves, native shader-program caches, settings or launcher are copied.
$env:RX_BACKEND = 'vulkan'
$env:HOS_HEADLESS = $(if ($Window) { '0' } else { '1' })
$env:HOS_WINDOW_NOACTIVATE = '1'
$env:HOS_FULLSCREEN = '0'
$env:HOS_AUDIO = '0'
$env:HOS_DISCORD = '0'
$env:HOS_OUTPUT_RES = '1280x720'
$env:HOS_HANDHELD = '1'
$env:RX_VK_FRAMEGEN = '0'
$env:HOOKSHOT_NO_UPDATE_CHECK = '1'
$env:HOS_MODS = Join-Path $runRoot 'mods'
$env:HOS_FS_OVERLAY = Join-Path $runRoot 'overlay'
$env:HOS_ENV_FILE = Join-Path $runRoot 'test_env.txt'
[IO.File]::WriteAllText($env:HOS_ENV_FILE, '# Isolated flat baseline; process environment controls this run.')
Remove-Item Env:HOS_CONTROL -ErrorAction SilentlyContinue
$env:RX_VK_SHOTS = Join-Path $runRoot 'shots'
$env:RX_VK_SHOT_EVERY = '120'
$exe = Join-Path $runRoot 'bin/hookshot.exe'
$argsForGame = @(('"' + (Join-Path $runRoot 'image.bin') + '"'), '--romfs',
                 ('"' + (Join-Path $InstallationRoot 'romfs') + '"'), '--user',
                 ('"' + (Join-Path $runRoot 'user') + '"'))
$p = Start-Process -FilePath $exe -ArgumentList $argsForGame -WorkingDirectory $runRoot -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $runRoot 'stdout.txt') -RedirectStandardError (Join-Path $runRoot 'stderr.txt')
try { $p.PriorityClass = 'BelowNormal' } catch { }
$reason = 'time_limit'
$peakMB = 0
try {
    for ($elapsed = 0; $elapsed -lt $Seconds; $elapsed += 2) {
        if ($p.WaitForExit(2000)) { $reason = 'process_exited'; break }
        $p.Refresh()
        $mb = [int]($p.WorkingSet64 / 1MB)
        $peakMB = [math]::Max($peakMB, $mb)
        if ($mb -gt ($MaximumWorkingSetGiB * 1024)) { $reason = 'memory_limit'; break }
        if ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($runRoot)).AvailableFreeSpace -lt 4GB) { $reason = 'disk_limit'; break }
    }
} finally {
    if (!$p.HasExited) { Stop-Process -Id $p.Id -Force; $p.WaitForExit() }
}
[ordered]@{ reason = $reason; exit_code = $p.ExitCode; peak_working_set_MB = $peakMB; runtime_root = $runRoot;
             headless = !$Window; gameplay_verified = $false } | ConvertTo-Json
