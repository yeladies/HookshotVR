# SPDX-License-Identifier: GPL-3.0-or-later
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows continuous stream receipt verifier')
class StreamReceipt(unittest.TestCase):
    def test_only_complete_moving_synthetic_diagnostic_is_accepted(self):
        with tempfile.TemporaryDirectory(prefix='vr-stream-receipt-') as directory:
            root=Path(directory);base=root/'sample'
            report={'schema':1,'recorded_pairs':130,'completed_pairs':129,'published_pairs':129,'failures':0,'allocated_bytes':4096,'probe_samples':3,'probe_attempts':3,'source_CPU_upload':False,'intermediate_CPU_pixels':False,'output_validation_readback':True,'synthetic_tracking':True,'XR_consumer_connected':False,'native_geometry_stereo':False,'width_per_eye':8,'height':8,'last_rgb_span':64,'last_eye_difference':32}
            source={'schema':1,'completed':130,'failures':0,'source_publication_CPU_pixels':False,'validation_packet_readback':False}
            immersive={'schema':1,'camera_overrides':240,'matched_draws':100,'synthetic_pose':True,'monoscopic':True,'temporal_stereo':False}
            for name,value in [('report.json',report),('source.json',source),('immersive.json',immersive)]: (root/name).write_text(json.dumps(value),encoding='utf-8')
            for n in (1,2,3): (root/f'sample-{n}.ppm').write_bytes(b'P6\n16 8\n255\n'+bytes([n])*384)
            quote=lambda x: "'"+str(x).replace("'","''")+"'"
            script=". "+quote(ROOT/'vr/gpu_eye_stream_probe.ps1')+"\n$report="+quote(root/'report.json')+"\n$source="+quote(root/'source.json')+"\n$immersive="+quote(root/'immersive.json')+"\n$base="+quote(base)+r'''
$script:checks=0
function Assert-Stream([bool]$expected){$script:checks++;$accepted=Test-VrGpuEyeStreamProbe -Report $report -ImageBase $base -SourceReport $source -ImmersiveReport $immersive;if($accepted -ne $expected){throw ('stream receipt mismatch at check '+$script:checks+' expected '+$expected+' actual '+$accepted)}}
Assert-Stream $true
$original=[IO.File]::ReadAllText($report)
foreach($mutation in @({$r.completed_pairs=2},{$r.probe_samples=2},{$r.failures=1},{$r.intermediate_CPU_pixels=$true},{$r.XR_consumer_connected=$true},{$r.synthetic_tracking=$false},{$r.last_rgb_span=14},{$r.last_eye_difference=0},{$r.allocated_bytes=67108865})){$r=$original|ConvertFrom-Json;& $mutation;[IO.File]::WriteAllText($report,($r|ConvertTo-Json));Assert-Stream $false}
[IO.File]::WriteAllText($report,$original)
Copy-Item -LiteralPath ($base+'-1.ppm') -Destination ($base+'-2.ppm') -Force
Copy-Item -LiteralPath ($base+'-1.ppm') -Destination ($base+'-3.ppm') -Force
Assert-Stream $false
Write-Output 'Continuous stream verifier rejects incomplete counts, false transport claims and unchanged samples'
'''
            path=root/'test.ps1';path.write_text(script,encoding='utf-8-sig')
            result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(path)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':unittest.main()
