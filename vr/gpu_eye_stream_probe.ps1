# SPDX-License-Identifier: GPL-3.0-or-later
function Test-VrGpuEyeStreamProbe {
    param([string]$Report,[string]$ImageBase,[string]$SourceReport,[string]$ImmersiveReport)
    try {
        foreach($path in @($Report,$SourceReport,$ImmersiveReport)){if(!(Test-Path -LiteralPath $path -PathType Leaf)){return $false}}
        $r=Get-Content -LiteralPath $Report -Raw -Encoding UTF8|ConvertFrom-Json
        $s=Get-Content -LiteralPath $SourceReport -Raw -Encoding UTF8|ConvertFrom-Json
        $i=Get-Content -LiteralPath $ImmersiveReport -Raw -Encoding UTF8|ConvertFrom-Json
        if($s.schema -ne 1 -or $s.completed -lt 120 -or $s.failures -ne 0 -or $s.source_publication_CPU_pixels -ne $false -or $s.validation_packet_readback -ne $false){return $false}
        if($i.schema -ne 1 -or $i.camera_overrides -lt 120 -or $i.matched_draws -le 0 -or $i.synthetic_pose -ne $true -or $i.monoscopic -ne $true -or $i.temporal_stereo -ne $false){return $false}
        if($r.schema -ne 1 -or $r.recorded_pairs -lt 120 -or $r.completed_pairs -lt 120 -or $r.published_pairs -lt 120 -or $r.failures -ne 0 -or $r.allocated_bytes -le 0 -or $r.allocated_bytes -gt 64MB -or $r.probe_samples -ne 3 -or $r.probe_attempts -lt 3 -or $r.probe_attempts -gt 12 -or $r.source_CPU_upload -ne $false -or $r.intermediate_CPU_pixels -ne $false -or $r.output_validation_readback -ne $true -or $r.synthetic_tracking -ne $true -or $r.XR_consumer_connected -ne $false -or $r.native_geometry_stereo -ne $false){return $false}
        if($r.width_per_eye -lt 8 -or $r.width_per_eye -gt 1920 -or $r.height -lt 8 -or $r.height -gt 1080 -or $r.width_per_eye%8 -ne 0 -or $r.height%8 -ne 0 -or $r.last_rgb_span -lt 32 -or $r.last_eye_difference -lt [math]::Max(1,[math]::Floor([double]$r.width_per_eye*$r.height/1000))){return $false}
        $header=[Text.Encoding]::ASCII.GetBytes("P6`n"+(2*$r.width_per_eye)+' '+$r.height+"`n255`n");$hashes=@()
        foreach($n in @(1,2,3)){
            $path=$ImageBase+'-'+$n+'.ppm';if(!(Test-Path -LiteralPath $path -PathType Leaf)){return $false}
            $stream=[IO.File]::OpenRead($path)
            try {$actual=New-Object byte[] $header.Length;$read=$stream.Read($actual,0,$actual.Length);if($read -ne $header.Length -or [BitConverter]::ToString($actual) -ne [BitConverter]::ToString($header) -or $stream.Length -ne ($header.Length+6*[uint64]$r.width_per_eye*[uint64]$r.height)){return $false}} finally {$stream.Dispose()}
            $algorithm=[Security.Cryptography.SHA256]::Create();$hashStream=[IO.File]::OpenRead($path)
            try {$hashes+=[BitConverter]::ToString($algorithm.ComputeHash($hashStream))} finally {$hashStream.Dispose();$algorithm.Dispose()}
        }
        return @($hashes|Select-Object -Unique).Count -ge 2
    } catch {return $false}
}
