# SPDX-License-Identifier: GPL-3.0-or-later
function Test-VrGpuWorldPreview {
    param([string]$Report,[string]$StreamReport,[string]$SourceReport)
    try {
        foreach($path in @($Report,$StreamReport,$SourceReport)){if(!(Test-Path -LiteralPath $path -PathType Leaf)){return $false}}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8|ConvertFrom-Json
        $p=Get-Content -LiteralPath $StreamReport -Raw -Encoding UTF8|ConvertFrom-Json
        $s=Get-Content -LiteralPath $SourceReport -Raw -Encoding UTF8|ConvertFrom-Json
        if($r.schema -ne 1 -or $r.status -ne 'closed' -or $r.gpu_pair_copies -le 0 -or $r.projection_frames_submitted -le 0 -or $r.CPU_pixel_staging -ne $false -or $r.production_pose_metadata -ne $true -or $r.same_source_state -ne $true -or $r.native_geometry_stereo -ne $false -or $r.pending -ne $false -or $r.acquired -ne $false -or $r.uncertain -ne $false -or $r.origin -le 0 -or $r.source_state -le 0 -or $r.device_generation -le 0){return $false}
        if($p.schema -ne 1 -or $p.completed_pairs -le 0 -or $p.published_pairs -le 0 -or $p.failures -ne 0 -or $p.allocated_bytes -gt 64MB -or $p.allocated_bytes -le 0 -or $p.source_CPU_upload -ne $false -or $p.intermediate_CPU_pixels -ne $false -or $p.output_validation_readback -ne $false -or $p.synthetic_tracking -ne $false -or $p.XR_consumer_connected -ne $true -or $p.native_geometry_stereo -ne $false){return $false}
        if($s.schema -ne 1 -or $s.completed -le 0 -or $s.failures -ne 0 -or $s.source_publication_CPU_pixels -ne $false -or $s.validation_packet_readback -ne $false){return $false}
        return $r.width_per_eye -eq $p.width_per_eye -and $r.height -eq $p.height -and $r.width_per_eye -ge 8 -and $r.width_per_eye -le 1920 -and $r.height -ge 8 -and $r.height -le 1080
    } catch {return $false}
}
