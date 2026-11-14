# SPDX-License-Identifier: GPL-3.0-or-later
function Test-VrGpuSourceProbe {
    param([string]$Report,[string]$Packet)
    try {
        if(!(Test-Path -LiteralPath $Report -PathType Leaf) -or !(Test-Path -LiteralPath $Packet -PathType Leaf)){return $false}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8 | ConvertFrom-Json
        if($r.schema -ne 1 -or !$r.probe_complete -or !$r.validation_packet_readback -or $r.source_publication_CPU_pixels -ne $false -or $r.XR_consumer_connected -ne $false -or $r.native_stereo -ne $false -or $r.failures -ne 0 -or $r.captures -lt 3 -or $r.completed -lt 3 -or $r.published -lt 3 -or $r.allocations -lt 2 -or $r.allocations -gt 4 -or $r.allocated_bytes -le 0 -or $r.allocated_bytes -gt 64MB -or $r.device_generation -le 0 -or $r.latest_state -le 0){return $false}
        $stream=[IO.File]::OpenRead($Packet)
        $reader=[IO.BinaryReader]::new($stream)
        try {
            $magic=$reader.ReadBytes(8)
            if([BitConverter]::ToString($magic) -ne '48-53-56-52-44-32-00-00'){return $false}
            [uint64]$width=$reader.ReadUInt32();[uint64]$height=$reader.ReadUInt32();$format=$reader.ReadUInt32()
            return $width -ge 2 -and $width -le 1920 -and $height -ge 2 -and $height -le 1080 -and $format -in @(122,123) -and $stream.Length -eq (132+8*$width*$height)
        } finally {$reader.Dispose();$stream.Dispose()}
    } catch {return $false}
}

function Test-VrGpuDepthPairProbe {
    param([string]$Report,[string]$Image,[string]$SourceReport,[switch]$SourceDebug)
    try {
        if(!(Test-Path -LiteralPath $Report -PathType Leaf) -or !(Test-Path -LiteralPath $Image -PathType Leaf) -or !(Test-Path -LiteralPath $SourceReport -PathType Leaf)){return $false}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8 | ConvertFrom-Json
        $s=Get-Content -LiteralPath $SourceReport -Raw -Encoding UTF8 | ConvertFrom-Json
        if($s.schema -ne 1 -or $s.completed -lt 3 -or $s.failures -ne 0 -or $s.source_publication_CPU_pixels -ne $false -or $s.validation_packet_readback -ne $false){return $false}
        if($r.schema -ne 1 -or $r.complete -ne $true -or $r.completed_pairs -ne 3 -or $r.recorded_pairs -lt 3 -or $r.recorded_pairs -gt 12 -or $r.failures -ne 0 -or $r.allocated_bytes -le 0 -or $r.allocated_bytes -gt 40MB -or $r.same_GPU_source_per_pair -ne $true -or $r.source_CPU_upload -ne $false -or $r.intermediate_CPU_pixels -ne $false -or $r.validation_output_readback -ne $true -or $r.XR -ne $false -or $r.native_geometry_stereo -ne $false -or $r.scale_calibrated -ne $false){return $false}
        if($r.content_verified -ne $true -or $r.rgb_span -lt 32 -or $r.different_eye_pixels -lt [math]::Max(1,[math]::Floor([double]$r.width_per_eye*$r.height/1000))){return $false}
        if($r.source_CPU_validation_readback -ne [bool]$SourceDebug -or ($SourceDebug -and (!$r.debug_source_written -or !$r.metadata_upload_verified))){return $false}
        if($r.width_per_eye -lt 2 -or $r.width_per_eye -gt 1920 -or $r.height -lt 2 -or $r.height -gt 1080 -or $r.source_states.Count -ne 3 -or $r.recordings.Count -ne 3 -or $r.last_counts.Count -ne 8){return $false}
        for($i=0;$i -lt 3;$i++){if($r.source_states[$i] -le 0 -or $r.recordings[$i] -le 0 -or ($i -gt 0 -and ($r.source_states[$i] -le $r.source_states[$i-1] -or $r.recordings[$i] -le $r.recordings[$i-1]))){return $false}}
        [uint64]$pixels=[uint64]$r.width_per_eye*[uint64]$r.height
        foreach($eye in @(0,1)){[uint64]$sum=0;for($i=0;$i -lt 4;$i++){if($r.last_counts[4*$eye+$i] -lt 0){return $false};$sum+=[uint64]$r.last_counts[4*$eye+$i]};if($sum -ne $pixels -or $r.last_counts[4*$eye] -le 0){return $false}}
        $header=[Text.Encoding]::ASCII.GetBytes("P6`n"+(2*$r.width_per_eye)+' '+$r.height+"`n255`n")
        $stream=[IO.File]::OpenRead($Image)
        try {$actual=New-Object byte[] $header.Length;$read=$stream.Read($actual,0,$actual.Length);return $read -eq $header.Length -and [BitConverter]::ToString($actual) -eq [BitConverter]::ToString($header) -and $stream.Length -eq ($header.Length+6*$pixels)} finally {$stream.Dispose()}
    } catch {return $false}
}
