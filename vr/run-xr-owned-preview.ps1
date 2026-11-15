# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180,[ValidateRange(0,2)][int]$GpuFramesAhead=0,[ValidateSet('Balanced','Sharp','Combined','Tuned','Performance')][string]$Profile='Balanced')
$ErrorActionPreference='Stop'
$size=if($Profile -eq 'Performance'){3200}elseif($Profile -eq 'Tuned'){3584}elseif($Profile -eq 'Combined'){4096}elseif($Profile -eq 'Sharp'){1920}else{1440}
$eye=if($Profile -in @('Tuned','Performance')){2304}elseif($Profile -eq 'Combined'){2560}else{$size}
$cull=if($Profile -eq 'Combined'){160}else{0}
& (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -GpuXrReproject -CoherentView:($Profile -in @('Tuned','Performance')) -GpuSquareSize $size -GpuEyeSize $eye -GpuFramesAhead $GpuFramesAhead -CullingFovDegrees $cull -Audio -Seconds $Seconds -PreviewResolution 1080p -FovMarginPercent 60 -GpuGuardBandPercent 8
exit $LASTEXITCODE
