# SPDX-License-Identifier: GPL-3.0-or-later
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows GPU world receipt verifier')
class WorldReceipt(unittest.TestCase):
    def test_rejects_incomplete_transport_and_retained_resources(self):
        with tempfile.TemporaryDirectory(prefix='vr-world-receipt-') as directory:
            root=Path(directory)
            r={'schema':1,'status':'closed','gpu_pair_copies':120,'projection_frames_submitted':160,'CPU_pixel_staging':False,'production_pose_metadata':True,'same_source_state':True,'native_geometry_stereo':False,'pending':False,'acquired':False,'uncertain':False,'origin':1,'source_state':23,'device_generation':2,'width_per_eye':1024,'height':1024}
            p={'schema':1,'completed_pairs':120,'published_pairs':120,'failures':0,'allocated_bytes':25166336,'source_CPU_upload':False,'intermediate_CPU_pixels':False,'output_validation_readback':False,'synthetic_tracking':False,'XR_consumer_connected':True,'native_geometry_stereo':False,'width_per_eye':1024,'height':1024}
            s={'schema':1,'completed':120,'failures':0,'source_publication_CPU_pixels':False,'validation_packet_readback':False}
            for name,value in [('world.json',r),('stream.json',p),('source.json',s)]: (root/name).write_text(json.dumps(value),encoding='utf-8')
            quote=lambda x:"'"+str(x).replace("'","''")+"'"
            script=". "+quote(ROOT/'vr/gpu_world_preview.ps1')+"\n$world="+quote(root/'world.json')+"\n$stream="+quote(root/'stream.json')+"\n$source="+quote(root/'source.json')+r'''
$checks=0
function Assert-World([bool]$expected){$actual=Test-VrGpuWorldPreview -Report $world -StreamReport $stream -SourceReport $source;if($actual -ne $expected){throw ('GPU world receipt mismatch '+$actual+' expected '+$expected)}}
Assert-World $true
$original=[IO.File]::ReadAllText($world)
foreach($mutation in @({$r.status='retained'},{$r.gpu_pair_copies=0},{$r.projection_frames_submitted=0},{$r.CPU_pixel_staging=$true},{$r.production_pose_metadata=$false},{$r.same_source_state=$false},{$r.pending=$true},{$r.acquired=$true},{$r.uncertain=$true},{$r.origin=0},{$r.width_per_eye=8})){$r=$original|ConvertFrom-Json;& $mutation;[IO.File]::WriteAllText($world,($r|ConvertTo-Json));Assert-World $false}
[IO.File]::WriteAllText($world,$original)
$original=[IO.File]::ReadAllText($stream)
foreach($mutation in @({$r.synthetic_tracking=$true},{$r.output_validation_readback=$true},{$r.XR_consumer_connected=$false},{$r.source_CPU_upload=$true},{$r.failures=1},{$r.allocated_bytes=67108865})){$r=$original|ConvertFrom-Json;& $mutation;[IO.File]::WriteAllText($stream,($r|ConvertTo-Json));Assert-World $false}
[IO.File]::WriteAllText($stream,$original)
$r=Get-Content -LiteralPath $source -Raw|ConvertFrom-Json;$r.validation_packet_readback=$true;[IO.File]::WriteAllText($source,($r|ConvertTo-Json));Assert-World $false
'''
            path=root/'test.ps1';path.write_text(script,encoding='utf-8-sig')
            result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(path)],capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
    def test_invalid_modes_fail_before_runtime_or_installation_access(self):
        for flags in [[],['-SnapshotStableSaves','-LoadTestSave','-TemporalStereo'],['-SnapshotStableSaves','-LoadTestSave','-GpuEyeStreamProbe'],['-SnapshotStableSaves','-LoadTestSave','-ImmersiveReadbackTest']]:
            result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'vr/run-game.ps1'),'-GpuWorldPreview','-Seconds','120','-InstallationRoot','Z:/nonexistent-vr-guard-input',*flags],capture_output=True,text=True,timeout=20)
            self.assertNotEqual(result.returncode,0)
            self.assertRegex(result.stderr,'GPU world preview (requires|cannot)')
if __name__=='__main__':unittest.main()
