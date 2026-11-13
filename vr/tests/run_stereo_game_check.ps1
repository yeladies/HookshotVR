# SPDX-License-Identifier: GPL-3.0-or-later
# Manual local-game check. Never opens XR; never a CI test or a game-data publisher.
param([ValidateRange(120,180)][int]$Seconds=180)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
if(Get-Process -Name hookshot -ErrorAction SilentlyContinue){throw 'Close the existing game before the isolated check.'}
$scope=Join-Path $project ('work/stereo-check-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scope | Out-Null
$started=[DateTime]::UtcNow
$stop=Join-Path $scope 'stop'
$child=$null;$before=$null;$during=$null;$after=$null
$reportPath=Join-Path $project 'work/game-run/immersive-result.json'
function Read-CurrentReport {
    try{
        if((Get-Item -LiteralPath $reportPath -ErrorAction Stop).LastWriteTimeUtc -lt $started){return $null}
        $value=Get-Content -LiteralPath $reportPath -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop
        if(!$value.synthetic_pose -or !$value.temporal_stereo){return $null}
        return $value
    }catch{return $null}
}
try{
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $project 'vr/run-game.ps1')+'"'),'-ImmersiveReadbackTest','-TemporalStereo','-SnapshotStableSaves','-LoadTestSave','-Seconds',$Seconds.ToString(),'-StopSignal',('"'+$stop+'"'))
    $child=Start-Process powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scope 'result.json') -RedirectStandardError (Join-Path $scope 'errors.txt')
    $null=$child.Handle
    $deadline=[DateTime]::UtcNow.AddSeconds([Math]::Min(140,$Seconds-20))
    do{
        Start-Sleep -Milliseconds 500;$child.Refresh()
        if($child.HasExited){throw ('Producer exited before stereo evidence: '+(Join-Path $scope 'errors.txt'))}
        $before=Read-CurrentReport
        if([DateTime]::UtcNow -gt $deadline){throw ('No current stereo/FOV proof before deadline: '+$scope)}
    }until($before -and $before.left_images -ge 30 -and $before.right_images -ge 30 -and $before.lens_updates -gt 0 -and [Math]::Abs($before.vertical_fov_rad-$before.requested_vertical_fov_rad) -lt .02)
    $before | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $scope 'before-menu.json') -Encoding UTF8
    $inputPath=Join-Path $project 'work/game-run/input-live.txt'
    [IO.File]::AppendAllText($inputPath,"0:PLUS/150`n")
    Start-Sleep -Seconds 6;$during=Read-CurrentReport
    $during | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $scope 'during-menu.json') -Encoding UTF8
    [IO.File]::AppendAllText($inputPath,"0:PLUS/150`n")
    Start-Sleep -Seconds 6;$after=Read-CurrentReport
    $after | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $scope 'after-menu.json') -Encoding UTF8
}finally{
    [IO.File]::WriteAllText($stop,'stop')
    if($child -and !$child.WaitForExit(15000)){
        $infoPath=$stop+'.process.json'
        if(Test-Path -LiteralPath $infoPath){
            $info=Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
            $native=Get-Process -Id $info.native_pid -ErrorAction SilentlyContinue
            $expected=Join-Path $project 'work/game-run/bin/hookshot.exe'
            if($native -and $native.Path -eq $expected -and $native.StartTime.ToUniversalTime().Ticks -eq ([DateTime]::Parse($info.started_utc)).ToUniversalTime().Ticks){Stop-Process -Id $native.Id -Force}
        }
        if(!$child.HasExited){Stop-Process -Id $child.Id -Force}
    }
}
$log=Get-Content -LiteralPath (Join-Path $project 'work/game-run/stderr.txt') -Raw
$distinct=$log -match 'xr-preview: temporal stereo pairs [1-9][0-9]*, distinct images 1'
$menu=$during -and $after -and $during.menu_images -gt $before.menu_images -and $after.matched_draws -gt $during.matched_draws
Copy-Item -LiteralPath $reportPath -Destination (Join-Path $scope 'camera-report.json')
$result=[ordered]@{schema=1;scope=$scope;stereo_ready=[bool]($before -and $distinct);distinct_pair_verified=$distinct;menu_transition_verified=[bool]$menu;before=$before;during=$during;after=$after;producer_exit_code=$child.ExitCode;headset_tested=$false}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $scope 'summary.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 5
if(!$result.stereo_ready -or $child.ExitCode -ne 0){exit 1}
