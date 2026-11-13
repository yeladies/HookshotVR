param([ValidateRange(5,60)][int]$Seconds=60,[switch]$Offscreen,[string]$ViewerExecutable='')
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
if(!$ViewerExecutable){$ViewerExecutable=Join-Path $project 'work/vr-build/hookshot_vr_scene.exe'}
$ViewerExecutable=(Resolve-Path -LiteralPath $ViewerExecutable).Path
$scope=Join-Path $project ('work/live-preview-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $scope | Out-Null
$frame=Join-Path $scope 'frame.hsvrd';$stop=Join-Path $scope 'stop'
$game=$null;$viewer=$null
Write-Host 'Preparando el juego experimental y el flujo de profundidad. La instalacion estable no se modifica.'
Write-Host ('Directorio privado de esta prueba: '+$scope)
if($Offscreen){Write-Host 'La verificacion GPU comenzara al llegar una captura reciente.'}
else{Write-Host 'Ponte las Quest ahora. El visor se abrira automaticamente al llegar una captura reciente; no selecciones texto en esta ventana.'}
try {
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $PSScriptRoot 'run-game.ps1')+'"'),'-Seconds','180','-ExperimentalXrRenderer','-DepthSceneStream','-DepthCapturePath',('"'+$frame+'"'),'-StopSignal',('"'+$stop+'"'),'-SnapshotStableSaves','-LoadTestSave')
    $game=Start-Process -FilePath 'powershell' -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scope 'game-result.json') -RedirectStandardError (Join-Path $scope 'game-errors.txt')
    $null=$game.Handle
    $deadline=[DateTime]::UtcNow.AddSeconds(120)
    while(!(Test-Path -LiteralPath $frame -PathType Leaf)){
        if($game.HasExited){throw ('No se obtuvo un frame. Revisa '+(Join-Path $scope 'game-errors.txt'))}
        if([DateTime]::UtcNow -gt $deadline){throw 'Tiempo de espera de la escena agotado.'}
        Start-Sleep -Milliseconds 500
    }
    if($Offscreen){
        $duration=[Math]::Min(30,$Seconds)
        $arguments=@('--stream-check',('"'+$frame+'"'),'--output',('"'+(Join-Path $scope 'eyes')+'"'),'--seconds',$duration.ToString())
    }else{
        $duration=$Seconds
        $arguments=@('--stream',('"'+$frame+'"'),'--seconds',$duration.ToString())
    }
    # Do not write to the console between producer startup and viewer startup:
    # a selected legacy console can block writes while the producer times out.
    $game.Refresh()
    if($game.HasExited){throw 'El productor termino antes de abrir el visor. No se mostrara una captura antigua.'}
    $latest=Get-Item -LiteralPath $frame
    if(([DateTime]::UtcNow-$latest.LastWriteTimeUtc).TotalSeconds -gt 2){
        throw 'La captura ya no es reciente. Repite la prueba sin seleccionar texto en la consola.'
    }
    $viewer=Start-Process -FilePath $ViewerExecutable -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scope 'viewer-result.json') -RedirectStandardError (Join-Path $scope 'viewer-errors.txt')
    $null=$viewer.Handle
    if(!$viewer.WaitForExit(($duration+30)*1000)){Stop-Process -Id $viewer.Id -Force;throw 'El visor supero el tiempo de la prueba.'}
    if($viewer.ExitCode -ne 0){throw ('El visor devolvio '+$viewer.ExitCode+'. Revisa '+(Join-Path $scope 'viewer-errors.txt'))}
}finally{
    [IO.File]::WriteAllText($stop,'stop')
    if($viewer -and !$viewer.HasExited){Stop-Process -Id $viewer.Id -Force}
    if($game -and !$game.WaitForExit(15000)){
        $infoPath=$stop+'.process.json'
        if(Test-Path -LiteralPath $infoPath){
            $info=Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
            $native=Get-Process -Id $info.native_pid -ErrorAction SilentlyContinue
            $expected=Join-Path $project 'work/game-run/bin/hookshot.exe'
            if($native -and $native.Path -eq $expected -and $native.StartTime.ToUniversalTime().Ticks -eq ([DateTime]::Parse($info.started_utc)).ToUniversalTime().Ticks){Stop-Process -Id $native.Id -Force}
        }
        if(!$game.HasExited){Stop-Process -Id $game.Id -Force}
    }
}
Get-Content -LiteralPath (Join-Path $scope 'viewer-result.json')
Write-Host ('Prueba terminada. Registros privados: '+$scope)
