param([ValidateRange(30,180)][int]$Seconds=120,[switch]$Mute)
$ErrorActionPreference='Stop'
Write-Output 'Quest preview: flat game screen in XR space; native stereo world rendering is not implemented yet.'
Write-Output 'Connect Virtual Desktop and wear the headset before starting. The test closes automatically.'
& (Join-Path $PSScriptRoot 'run-game.ps1') -Seconds $Seconds -XrPreview -SnapshotStableSaves -LoadTestSave -Audio:(!$Mute)
exit $LASTEXITCODE