# SPDX-License-Identifier: GPL-3.0-or-later
"""Validate the actual PowerShell field verifier with authored lens data."""
from pathlib import Path
import subprocess, tempfile, shutil, unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows receipt verifier')
class VisibilityFields(unittest.TestCase):
    def test_split_volume_and_invalid_observations(self):
        with tempfile.TemporaryDirectory(prefix='vr-visibility-') as directory:
            root=Path(directory); report=root/'report.json'
            script=". '"+str(ROOT/'vr/gpu_world_preview.ps1').replace("'","''")+"'\n$path='"+str(report).replace("'","''")+"'\n"+r'''
$degrees=360/[math]::PI;$raster=2.28;$aspect=.96375;$visibility=[math]::Tan(160/$degrees)/$aspect
$original=[pscustomobject]@{schema=1;field_observations=120;raster_tangent=$raster;visibility_tangent=$visibility;raster_vertical_degrees=([math]::Atan($raster)*$degrees);raster_horizontal_degrees=([math]::Atan($raster*$aspect)*$degrees);visibility_vertical_degrees=([math]::Atan($visibility)*$degrees);visibility_horizontal_degrees=([math]::Atan($visibility*$aspect)*$degrees);complete_culling_coverage_proven=$false}|ConvertTo-Json
[IO.File]::WriteAllText($path,$original)
if(!(Test-VrVisibilityFields -Report $path -RequestedDegrees 160)){throw 'valid split visibility fields rejected'}
foreach($change in @({$r.field_observations=0},{$r.complete_culling_coverage_proven=$true},{$r.visibility_tangent=$r.raster_tangent},{$r.visibility_horizontal_degrees=159},{$r.visibility_tangent=16},{$r.raster_vertical_degrees='NaN'},{$r.visibility_vertical_degrees=170})){
$r=$original|ConvertFrom-Json;&$change;[IO.File]::WriteAllText($path,($r|ConvertTo-Json));if(Test-VrVisibilityFields -Report $path -RequestedDegrees 160){throw 'invalid/inconsistent observation accepted'}}
'''
            path=root/'test.ps1';path.write_text(script,encoding='utf-8-sig')
            r=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(path)],capture_output=True,text=True,timeout=30)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
if __name__=='__main__':unittest.main()
