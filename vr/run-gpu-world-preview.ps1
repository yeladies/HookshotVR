# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'run-immersive-preview.ps1') -GpuWorldPreview -Audio -Seconds $Seconds
exit $LASTEXITCODE
