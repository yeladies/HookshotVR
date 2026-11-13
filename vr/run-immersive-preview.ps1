param([ValidateRange(120,180)][int]$Seconds=180,[switch]$Window,[switch]$Audio,[switch]$WideFov,[switch]$TemporalStereo)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$mutex=[Threading.Mutex]::new($false,'Local\HookshotVR-Immersive-Preview')
if(!$mutex.WaitOne(0)){$mutex.Dispose();throw 'Ya hay una prueba inmersiva abierta.'}
$child=$null;$scope=$null
try{
    $native=Join-Path $project 'work/game-run/bin/hookshot.exe'
    if(Get-Process -Name hookshot -ErrorAction SilentlyContinue){throw 'Cierra la otra instancia de Hookshot antes de esta prueba.'}
    $scope=Join-Path $project ('work/immersive-preview-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $scope | Out-Null
    $stop=Join-Path $scope 'stop'
    if($TemporalStereo){Write-Host 'FOV Y ESTEREO TEMPORAL EXPERIMENTAL - DOS VISTAS EN FOTOGRAMAS PROXIMOS'}else{Write-Host 'JUEGO REAL CON CAMARA DE CABEZA - PROTOTIPO MONOSCOPICO'}
    Write-Host 'Conecta Virtual Desktop y ponte las Quest. Puede tardar 90 segundos en cargar la partida.'
    if($TemporalStereo){Write-Host 'Puede haber diferencias temporales en objetos en movimiento. Empieza mirando una escena quieta.'}else{Write-Host 'Ambos ojos reciben la misma vista.'}
    Write-Host 'Tercera persona; sin controles fisicos VR. La prueba se cerrara automaticamente.'
    Write-Host 'Prueba giros pequenos y desplazamientos leves. Se cerrara automaticamente.'
    Write-Host ('Registros privados: '+$scope)
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $PSScriptRoot 'run-game.ps1')+'"'),'-ImmersivePreview','-SnapshotStableSaves','-LoadTestSave','-Seconds',$Seconds.ToString(),'-StopSignal',('"'+$stop+'"'))
    if($Window){$arguments+='-Window'}
    if($Audio){$arguments+='-Audio'}
    if($WideFov){$arguments+='-WideFov'}
    if($TemporalStereo){$arguments+='-TemporalStereo'}
    $child=Start-Process powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $scope 'result.json') -RedirectStandardError (Join-Path $scope 'errors.txt')
    $null=$child.Handle
    # Keep console writes outside the active child lifetime: console selection must
    # not delay the startup or watchdog of the game/renderer session.
    if(!$child.WaitForExit(($Seconds+30)*1000)){throw 'La prueba supero su tiempo limite.'}
    if($child.ExitCode -ne 0){throw ('La prueba no se completo. Revisa '+(Join-Path $scope 'errors.txt'))}
}finally{
    if($scope){
        [IO.File]::WriteAllText((Join-Path $scope 'stop'),'stop')
        if($child -and !$child.WaitForExit(8000)){
            $infoPath=Join-Path $scope 'stop.process.json'
            if(Test-Path -LiteralPath $infoPath){
                $info=Get-Content -LiteralPath $infoPath -Raw | ConvertFrom-Json
                $process=Get-Process -Id $info.native_pid -ErrorAction SilentlyContinue
                if($process -and $process.Path -eq $native -and $process.StartTime.ToUniversalTime().Ticks -eq ([DateTime]::Parse($info.started_utc)).ToUniversalTime().Ticks){Stop-Process -Id $process.Id -Force}
            }
            if(!$child.HasExited){Stop-Process -Id $child.Id -Force}
        }
    }
    $mutex.ReleaseMutex();$mutex.Dispose()
}
Get-Content -LiteralPath (Join-Path $scope 'result.json')
Write-Host 'Prueba terminada. Comenta si la imagen sigue los giros y traslaciones de cabeza.'
