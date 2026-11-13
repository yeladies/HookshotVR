param([string]$MeshPath='', [ValidateRange(1,90)][int]$Seconds=60)
$ErrorActionPreference='Stop'
$packaged=Test-Path -LiteralPath (Join-Path $PSScriptRoot 'hookshot_vr_scene.exe') -PathType Leaf
$project=if($packaged){$PSScriptRoot}else{[IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))}
if(!$MeshPath){$MeshPath=Join-Path $project $(if($packaged){'scene.hsvrm'}else{'work/validation/depth-scene-preview.hsvrm'})}
$MeshPath=(Resolve-Path -LiteralPath $MeshPath).Path
$exe=Join-Path $project $(if($packaged){'hookshot_vr_scene.exe'}else{'work/vr-build/hookshot_vr_scene.exe'})
if(!(Test-Path -LiteralPath $exe -PathType Leaf)){throw 'Build the VR scene diagnostic first.'}
$log=Join-Path $project ($(if($packaged){'depth-headset-'}else{'work/validation/depth-headset-'})+[DateTime]::Now.ToString('yyyyMMdd-HHmmss'))
Write-Host 'Prueba 3D congelada de Hookshot: conecta Quest con Virtual Desktop y ponte el visor.'
Write-Host 'Valida profundidad, suelo y respuesta al inclinar la cabeza. La escena no avanza y tiene huecos fuera de la captura.'
$process=Start-Process -FilePath $exe -ArgumentList @('--mesh',('"'+$MeshPath+'"'),'--seconds',$Seconds.ToString()) -WindowStyle Hidden -PassThru -RedirectStandardOutput ($log+'.json') -RedirectStandardError ($log+'.stderr.txt')
$null=$process.Handle
if(!$process.WaitForExit(($Seconds+30)*1000)){Stop-Process -Id $process.Id -Force;throw 'La prueba superó su tiempo máximo y se cerró.'}
Get-Content -LiteralPath ($log+'.json')
if($process.ExitCode -ne 0){throw "El visor devolvió $($process.ExitCode). Registro: $log.stderr.txt"}
