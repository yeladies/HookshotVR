# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180,[ValidateSet('Baseline','Clarity')][string]$Profile='Clarity')
$ErrorActionPreference='Stop'
if($Profile -eq 'Clarity'){
    & (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -Audio -Seconds $Seconds -PreviewResolution 1080p -FovMarginPercent 25 -GpuGuardBandPercent 10
}else{
    & (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -Audio -Seconds $Seconds
}
exit $LASTEXITCODE
