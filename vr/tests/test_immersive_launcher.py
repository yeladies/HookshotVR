"""Reject unsafe mode combinations before reading game files or opening XR."""
from pathlib import Path
import shutil
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows launcher fixture')
class ImmersiveLaunchGuards(unittest.TestCase):
    def reject(self,args,message):
        result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'vr/run-game.ps1'),*args],capture_output=True,text=True,timeout=20)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(message,result.stdout+result.stderr)
    def test_lens_and_stereo_require_isolated_immersive_mode(self):
        for mode in ('-WideFov','-TemporalStereo'):
            self.reject([mode],'Wide FOV/stereo require an isolated immersive mode')
    def test_synthetic_cannot_open_xr(self):
        for switch in ('-XrPreview','-XrSessionProbe','-XrNegotiation'):
            with self.subTest(switch=switch):self.reject(['-Seconds','180','-ImmersiveReadbackTest',switch],'Synthetic head motion cannot be combined with XR')
    def test_both_modes_rejected(self):
        self.reject(['-Seconds','180','-ImmersivePreview','-ImmersiveReadbackTest'],'Synthetic head motion must not open XR')
    def test_competing_capture_rejected(self):
        self.reject(['-Seconds','180','-ImmersivePreview','-DepthSceneCapture'],'Immersive camera needs its own isolated run')
    def test_insufficient_preparation_time_rejected(self):
        self.reject(['-Seconds','60','-ImmersivePreview'],'immersive test needs at least 120 seconds')
if __name__=='__main__':unittest.main()
