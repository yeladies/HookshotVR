# SPDX-License-Identifier: GPL-3.0-or-later
param([ValidateRange(120,180)][int]$Seconds=180,[ValidateRange(0,2)][int]$GpuFramesAhead=0,[ValidateSet('Balanced','Sharp','Combined','Tuned','Performance')][string]$Profile='Balanced')
$ErrorActionPreference='Stop'
throw 'Depth camera retired on this branch; preserved in vr/archive-depth-camera-20261115. Native stereo integration is not ready for a headset test.'
