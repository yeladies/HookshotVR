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
if __name__=='__main__':unittest.main()
