# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180,[ValidateSet('Balanced','Sharp','Combined')][string]$Profile='Balanced')
$ErrorActionPreference='Stop'
$size=if($Profile -eq 'Combined'){4096}elseif($Profile -eq 'Sharp'){1920}else{1440}
$eye=if($Profile -eq 'Combined'){2560}else{$size}
$cull=if($Profile -eq 'Combined'){160}else{0}
& (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -GpuXrReproject -GpuSquareSize $size -GpuEyeSize $eye -CullingFovDegrees $cull -Audio -Seconds $Seconds -PreviewResolution 1080p -FovMarginPercent 60 -GpuGuardBandPercent 8
exit $LASTEXITCODE
