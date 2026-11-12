function Get-VrFileHash([string]$Path) {
    $stream=[IO.File]::OpenRead($Path)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','') }
    finally { $sha.Dispose();$stream.Dispose() }
}
# Source-development compatibility guards; never writes the installed game.
function Get-VrManifest([string]$ProjectRoot) {
    $manifest = Get-Content -LiteralPath (Join-Path $ProjectRoot 'vr/upstream.json') -Raw | ConvertFrom-Json
    $header = [IO.File]::ReadAllText((Join-Path $ProjectRoot 'gpu/vk/vk_api.h'))
    $match = [regex]::Match($header, '#define\s+MKWVK_API_VERSION\s+(\d+)')
    if (!$match.Success -or [int]$match.Groups[1].Value -ne [int]$manifest.mod_host_renderer_abi) {
        throw 'Source ABI does not match the reviewed maintenance manifest. Review the upstream integration first.'
    }
    if ($manifest.schema -ne 1 -or !$manifest.supported_game_builds.Count) { throw 'Invalid VR compatibility manifest.' }
    return $manifest
}
function Assert-VrSupportedInstallation([string]$ProjectRoot,[string]$InstallationRoot) {
    $manifest = Get-VrManifest $ProjectRoot
    $stream = [IO.File]::OpenRead((Join-Path $InstallationRoot '.hookshot_exefs/main'))
    try {
        $magic = New-Object byte[] 4
        if ($stream.Read($magic,0,4) -ne 4 -or [Text.Encoding]::ASCII.GetString($magic) -ne 'NSO0') { throw 'Unsupported program header.' }
        $stream.Position=64;$id=New-Object byte[] 32
        if ($stream.Read($id,0,32) -ne 32) { throw 'Program build ID is truncated.' }
        $buildId=([BitConverter]::ToString($id)).Replace('-','').ToLowerInvariant()
    } finally { $stream.Dispose() }
    $supported=$false
    foreach($expected in $manifest.supported_game_builds) {
        if ($expected -notmatch '^[0-9a-f]{40}([0-9a-f]{24})?$') { throw 'Malformed supported build ID in manifest.' }
        if($buildId.StartsWith($expected,[StringComparison]::Ordinal)) { $supported=$true }
    }
    if(!$supported) { throw 'The installed game build is not supported by this VR source. Prepare and validate an upstream candidate before rebuilding.' }
    return $buildId
}
function Get-VrPython([string]$BuilderToolsRoot) {
    if($BuilderToolsRoot) { $path=Join-Path $BuilderToolsRoot 'python/python.exe'; if(Test-Path -LiteralPath $path){return $path} }
    $versions=Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions'
    $path=Get-ChildItem -LiteralPath $versions -Directory | Sort-Object LastWriteTime -Descending | ForEach-Object {
        $candidate=Join-Path $_.FullName 'HookshotBuilder/tools/python/python.exe';if(Test-Path -LiteralPath $candidate){$candidate}
    } | Select-Object -First 1
    if(!$path){throw 'Builder Python is required for the development-source compatibility check.'}
    return $path
}
function Get-VrSourceFingerprint([string]$ProjectRoot,[string]$Python) {
    $value=& $Python -I -B -c 'import sys;sys.path.insert(0,sys.argv[1]);import version;print(version.fingerprint())' (Join-Path $ProjectRoot 'nx2')
    if($LASTEXITCODE -ne 0 -or !$value){throw 'Could not fingerprint development sources.'}
    return ([string]$value).Trim()
}
function Write-VrBuildReceipt([string]$ProjectRoot,[string]$Receipt,[string]$Renderer,[string]$HostExe,[string]$Fingerprint,[bool]$OpenXR) {
    $manifest=Get-VrManifest $ProjectRoot
    $data=[ordered]@{schema=1;abi=[int]$manifest.mod_host_renderer_abi;source_fingerprint=$Fingerprint;openxr_compiled=$OpenXR;
        supported_game_builds=@($manifest.supported_game_builds);renderer_sha256=(Get-VrFileHash $Renderer);
        host_sha256=$(if($HostExe){(Get-VrFileHash $HostExe)}else{$null})}
    $data | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $Receipt -Encoding UTF8
}
function Assert-VrBuildReceipt([string]$ProjectRoot,[string]$Receipt,[string]$Renderer,[string]$HostExe,[string]$Fingerprint,[bool]$RequireOpenXR) {
    $manifest=Get-VrManifest $ProjectRoot
    if(!(Test-Path -LiteralPath $Receipt)){throw 'Build receipt missing. Rebuild the experimental binaries with the current scripts.'}
    $data=Get-Content -LiteralPath $Receipt -Raw | ConvertFrom-Json
    if($data.schema -ne 1 -or $data.abi -ne $manifest.mod_host_renderer_abi -or $data.source_fingerprint -ne $Fingerprint) {
        throw 'Experimental build is stale or has a different ABI. Rebuild before launching.'
    }
    if(($data.supported_game_builds -join ',') -ne ($manifest.supported_game_builds -join ',')){throw 'Build receipt has a different supported-game profile.'}
    if($data.renderer_sha256 -ne (Get-VrFileHash $Renderer)){throw 'Renderer hash differs from its build receipt.'}
    if($HostExe -and $data.host_sha256 -ne (Get-VrFileHash $HostExe)){throw 'Host executable hash differs from its build receipt.'}
    if($RequireOpenXR -and !$data.openxr_compiled){throw 'This test requires a renderer built with the reviewed OpenXR SDK option.'}
}