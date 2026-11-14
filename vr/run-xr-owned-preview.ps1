# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180,[ValidateSet('Balanced','Sharp')][string]$Profile='Balanced')
$ErrorActionPreference='Stop'
$size=if($Profile -eq 'Sharp'){1920}else{1440}
& (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -GpuXrReproject -GpuSquareSize $size -Audio -Seconds $Seconds -PreviewResolution 1080p -FovMarginPercent 30 -GpuGuardBandPercent 8
exit $LASTEXITCODE
