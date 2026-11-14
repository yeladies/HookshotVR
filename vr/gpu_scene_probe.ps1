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
