# SPDX-License-Identifier: GPL-3.0-or-later
function Test-VrGpuWorldPreview {
    param([string]$Report,[string]$StreamReport,[string]$SourceReport,[double]$ExpectedEyeScale=1,[bool]$XrOwned=$false)
    try {
        foreach($path in @($Report,$SourceReport)){if(!(Test-Path -LiteralPath $path -PathType Leaf)){return $false}}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8|ConvertFrom-Json
        $p=$null;if(!$XrOwned){$p=Get-Content -LiteralPath $StreamReport -Raw -Encoding UTF8|ConvertFrom-Json}
        $s=Get-Content -LiteralPath $SourceReport -Raw -Encoding UTF8|ConvertFrom-Json
        if($r.schema -ne 1 -or $r.status -ne 'closed' -or $r.gpu_pair_copies -le 0 -or $r.projection_frames_submitted -le 0 -or $r.CPU_pixel_staging -ne $false -or $r.production_pose_metadata -ne $true -or $r.same_source_state -ne $true -or $r.native_geometry_stereo -ne $false -or $r.pending -ne $false -or $r.acquired -ne $false -or $r.uncertain -ne $false -or $r.origin -le 0 -or $r.source_state -le 0 -or $r.device_generation -le 0){return $false}
        if(!$XrOwned){
        if($p.schema -ne 1 -or $p.completed_pairs -le 0 -or $p.published_pairs -le 0 -or $p.failures -ne 0 -or $p.allocated_bytes -gt 256MB -or $p.allocated_bytes -le 0 -or $p.source_CPU_upload -ne $false -or $p.intermediate_CPU_pixels -ne $false -or $p.output_validation_readback -ne $false -or $p.synthetic_tracking -ne $false -or $p.XR_consumer_connected -ne $true -or $p.native_geometry_stereo -ne $false){return $false}
        if($p.depth_aware_color_filter -ne $true -or !$p.PSObject.Properties['eye_tangent_scale'] -or [math]::Abs([double]$p.eye_tangent_scale-$ExpectedEyeScale) -gt .0001){return $false}
        }elseif($r.depth_aware_color_filter -ne $true -or $r.background_repair_radius -ne 8 -or [math]::Abs([double]$r.eye_tangent_scale-$ExpectedEyeScale) -gt .0001 -or $r.reprojection_allocated_bytes -le 0 -or $r.reprojection_allocated_bytes -gt 64MB -or $r.reprojection_recorded -lt $r.gpu_pair_copies -or $r.xr_owned_reconstruction -ne $true -or $r.presentation_timing.samples -ne $r.projection_frames_submitted -or $r.presentation_timing.invalid_samples -ne 0){return $false}
        if($s.schema -ne 1 -or $s.completed -le 0 -or $s.failures -ne 0 -or $s.source_publication_CPU_pixels -ne $false -or $s.validation_packet_readback -ne $false){return $false}
        return ($XrOwned -or ($r.width_per_eye -eq $p.width_per_eye -and $r.height -eq $p.height)) -and $r.width_per_eye -ge 8 -and $r.width_per_eye -le 2560 -and $r.height -ge 8 -and $r.height -le 2560 -and ([uint64]$r.width_per_eye*$r.height -le 6553600)
    } catch {return $false}
}

function Test-VrVisibilityFields {
    param([string]$Report,[int]$RequestedDegrees)
    try {
        if($RequestedDegrees -lt 90 -or $RequestedDegrees -gt 160){return $false}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8|ConvertFrom-Json
        foreach($key in @('raster_vertical_degrees','raster_horizontal_degrees','visibility_vertical_degrees','visibility_horizontal_degrees','raster_tangent','visibility_tangent')){
            $v=[double]$r.$key;if(!$r.PSObject.Properties[$key] -or [double]::IsNaN($v) -or [double]::IsInfinity($v)){return $false}}
        $degrees=360/[math]::PI
        $aspect=[math]::Tan([double]$r.raster_horizontal_degrees/$degrees)/[double]$r.raster_tangent
        if($aspect -lt .5 -or $aspect -gt 2.5 -or [math]::Abs([math]::Atan([double]$r.raster_tangent)*$degrees-$r.raster_vertical_degrees) -gt .001 -or [math]::Abs([math]::Atan([double]$r.visibility_tangent)*$degrees-$r.visibility_vertical_degrees) -gt .001 -or [math]::Abs([math]::Atan([double]$r.visibility_tangent*$aspect)*$degrees-$r.visibility_horizontal_degrees) -gt .001){return $false}
        return $r.schema -eq 1 -and $r.field_observations -gt 0 -and $r.complete_culling_coverage_proven -eq $false -and $r.raster_tangent -gt .2 -and $r.raster_tangent -lt 2.8 -and $r.visibility_tangent -gt $r.raster_tangent -and $r.visibility_tangent -lt 16 -and $r.visibility_vertical_degrees -ge ($RequestedDegrees-.01) -and $r.visibility_horizontal_degrees -ge ($RequestedDegrees-.01) -and $r.visibility_vertical_degrees -lt 175 -and $r.visibility_horizontal_degrees -lt 175 -and $r.raster_vertical_degrees -lt $r.visibility_vertical_degrees -and $r.raster_horizontal_degrees -lt $r.visibility_horizontal_degrees
    }catch{return $false}
}
