"""Retired cameras must fail before touching an installation or opening XR."""
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows launcher fixture')
class CameraRouteRetirement(unittest.TestCase):
    def rejected(self,script,args):
        result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'vr'/script),*args],capture_output=True,text=True,timeout=20)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('Depth camera retired',result.stdout+result.stderr)
        self.assertNotIn('Cannot find path',result.stdout+result.stderr)
    def test_retired_game_modes_never_reach_installation(self):
        for flag in ('ImmersivePreview','WideFov','TemporalStereo','ImmersiveReadbackTest','GpuWorldPreview','GpuXrReproject','CoherentView','GpuEyeStreamProbe','GpuDepthPairProbe','GpuSceneSourceProbe','GpuSquareSource'):
            with self.subTest(flag=flag):self.rejected('run-game.ps1',['-InstallationRoot','Z:/not-an-installation','-'+flag])
    def test_retired_profiles_and_wrappers_never_launch(self):
        for profile in ('Balanced','Sharp','Combined','Tuned','Performance'):
            with self.subTest(profile=profile):self.rejected('run-xr-owned-preview.ps1',['-Profile',profile])
        self.rejected('run-immersive-preview.ps1',[])
        self.rejected('run-immersive-preview.ps1',['-GpuWorldPreview','-GpuXrReproject'])
class NativeCameraRetirement(unittest.TestCase):
    def test_original_game_writers_and_ultrawide_preserved(self):
        host=(ROOT/'host/hos_main.c').read_text(encoding='utf-8')
        self.assertIn('void hos_oot_free_view(',host)
        self.assertIn('oot_build_free_view(c, enabled ? view : 0, view + 8)',host)
        self.assertNotIn('RX_VK_XR_IMMERSIVE',host);self.assertNotIn('hos_vr_lens_parameters',host)
        wide=(ROOT/'nx2/oot_wide.py').read_text(encoding='utf-8')
        self.assertIn('hos_wide_ratio',wide);self.assertNotIn('hos_vr_',wide)
        applet=(ROOT/'host/nn_applet.c').read_text(encoding='utf-8');self.assertNotIn('HOS_VR_SQUARE_SOURCE',applet)
        renderer=(ROOT/'gpu/vk/vk_main.cpp').read_text(encoding='utf-8')
        self.assertIn('table.head_camera = nullptr;',renderer);self.assertIn('table.head_lens = nullptr;',renderer)
    def test_receipt_fingerprint_tracks_separate_camera_source(self):
        spec=importlib.util.spec_from_file_location('camera_version',ROOT/'nx2/version.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory(prefix='vr-fingerprint-') as folder:
            root=Path(folder);(root/'vr/src').mkdir(parents=True);p=root/'vr/src/vr_camera.c';p.write_text('first\n',encoding='utf-8')
            before=module.fingerprint(root);p.write_text('second\n',encoding='utf-8');self.assertNotEqual(before,module.fingerprint(root))
if __name__=='__main__':unittest.main()
