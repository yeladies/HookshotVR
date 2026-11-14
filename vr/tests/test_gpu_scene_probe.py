# SPDX-License-Identifier: GPL-3.0-or-later
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows receipt verifier')
class ProbeReceipt(unittest.TestCase):
    def test_incomplete_or_misleading_receipts_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='vr-source-receipt-') as directory:
            root=Path(directory)
            report={'schema':1,'probe_complete':True,'validation_packet_readback':True,'source_publication_CPU_pixels':False,'XR_consumer_connected':False,'native_stereo':False,'failures':0,'captures':3,'completed':3,'published':3,'allocations':4,'allocated_bytes':512,'device_generation':1,'latest_state':3}
            (root/'report.json').write_text(json.dumps(report),encoding='utf-8')
            (root/'packet.hsvrd').write_bytes(struct.pack('<8sIII',b'HSVRD2\0\0',4,4,123)+bytes(112+128))
            quote=lambda x: "'"+str(x).replace("'","''")+"'"
            script=". "+quote(ROOT/'vr/gpu_scene_probe.ps1')+"\n$report="+quote(root/'report.json')+"\n$packet="+quote(root/'packet.hsvrd')+r'''
function Assert-Receipt([bool]$expected){if((Test-VrGpuSourceProbe -Report $report -Packet $packet) -ne $expected){throw 'receipt acceptance mismatch'}}
Assert-Receipt $true
$original=[IO.File]::ReadAllText($report)
foreach($case in @(@('completed',2),@('published',0),@('failures',1),@('allocations',5),@('allocated_bytes',67108865),@('probe_complete',$false),@('source_publication_CPU_pixels',$true),@('XR_consumer_connected',$true),@('native_stereo',$true))){$r=$original|ConvertFrom-Json;$r.($case[0])=$case[1];[IO.File]::WriteAllText($report,($r|ConvertTo-Json));Assert-Receipt $false}
[IO.File]::WriteAllText($report,$original)
$bytes=[IO.File]::ReadAllBytes($packet)
$bytes[0]=0;[IO.File]::WriteAllBytes($packet,$bytes);Assert-Receipt $false
$bytes[0]=72;$bytes[16]=124;[IO.File]::WriteAllBytes($packet,$bytes);Assert-Receipt $false
$bytes[16]=123;[IO.File]::WriteAllBytes($packet,$bytes[0..258]);Assert-Receipt $false
Write-Output 'Actual verifier rejects incomplete publication and malformed packets'
'''
            path=root/'test.ps1';path.write_text(script,encoding='utf-8-sig')
            result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(path)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_live_pair_receipt_rejects_temporal_or_cpu_claims(self):
        with tempfile.TemporaryDirectory(prefix='vr-live-pair-receipt-') as directory:
            root=Path(directory)
            report={'schema':1,'complete':True,'completed_pairs':3,'recorded_pairs':3,'failures':0,'allocated_bytes':512,'content_verified':True,'rgb_span':64,'different_eye_pixels':16,'source_CPU_validation_readback':False,'same_GPU_source_per_pair':True,'source_CPU_upload':False,'intermediate_CPU_pixels':False,'validation_output_readback':True,'XR':False,'native_geometry_stereo':False,'scale_calibrated':False,'width_per_eye':4,'height':4,'source_states':[1,2,3],'recordings':[15,35,55],'last_counts':[16,0,0,0,16,0,0,0]}
            source={'schema':1,'completed':3,'failures':0,'source_publication_CPU_pixels':False,'validation_packet_readback':False}
            (root/'pair.json').write_text(json.dumps(report),encoding='utf-8');(root/'source.json').write_text(json.dumps(source),encoding='utf-8')
            (root/'pair.ppm').write_bytes(b'P6\n8 4\n255\n'+bytes(96))
            quote=lambda x: "'"+str(x).replace("'","''")+"'"
            script=". "+quote(ROOT/'vr/gpu_scene_probe.ps1')+"\n$report="+quote(root/'pair.json')+"\n$image="+quote(root/'pair.ppm')+"\n$source="+quote(root/'source.json')+r'''
function Assert-Pair([bool]$expected){if((Test-VrGpuDepthPairProbe -Report $report -Image $image -SourceReport $source) -ne $expected){throw 'pair receipt acceptance mismatch'}}
Assert-Pair $true
$original=[IO.File]::ReadAllText($report)
$mutations=@({$r.content_verified=$false},{$r.rgb_span=14},{$r.different_eye_pixels=0},{$r.source_CPU_upload=$true},{$r.same_GPU_source_per_pair=$false},{$r.completed_pairs=2},{$r.intermediate_CPU_pixels=$true},{$r.source_states=@(1,1,3)},{$r.recordings=@(15,14,55)},{$r.last_counts=@(15,0,0,0,16,0,0,0)},{$r.XR=$true},{$r.native_geometry_stereo=$true})
foreach($mutation in $mutations){$r=$original|ConvertFrom-Json;& $mutation;[IO.File]::WriteAllText($report,($r|ConvertTo-Json));Assert-Pair $false}
[IO.File]::WriteAllText($report,$original)
$s=Get-Content $source -Raw|ConvertFrom-Json;$s.validation_packet_readback=$true;[IO.File]::WriteAllText($source,($s|ConvertTo-Json));Assert-Pair $false
Write-Output 'Live pair verifier rejects repeated states, CPU transport claims and incomplete pairs'
'''
            path=root/'test.ps1';path.write_text(script,encoding='utf-8-sig')
            result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(path)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
