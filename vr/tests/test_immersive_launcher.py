"""Reject unsafe mode combinations before reading game files or opening XR."""
from pathlib import Path
import shutil
import json
import sys
import tempfile
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[2]
@unittest.skipUnless(shutil.which('powershell'),'Windows launcher fixture')
class ImmersiveLaunchGuards(unittest.TestCase):
    def reject(self,args,message):
        result=subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'vr/run-game.ps1'),*args],capture_output=True,text=True,timeout=20)
        self.assertNotEqual(result.returncode,0)
        self.assertIn(message,result.stdout+result.stderr)
    def test_gpu_source_requires_private_save(self):
        self.reject(['-GpuSceneSourceProbe'],'GPU scene source probe requires a private test save')
    def test_gpu_source_cannot_open_xr_or_share_capture(self):
        for flag in ('-XrPreview','-TemporalStereo','-DepthSceneCapture','-GpuRootAliasProbe'):
            self.reject(['-GpuSceneSourceProbe','-SnapshotStableSaves','-LoadTestSave','-Seconds','120',flag],'GPU scene source probe requires its own non-XR run')
    def test_lens_selection_flag_is_forwarded_as_one_native_argument(self):
        source=(ROOT/'vr/run-game.ps1').read_text(encoding='utf-8-sig')
        assignment=next(line for line in source.splitlines() if 'shiftSelectionArgs=if(' in line)
        with tempfile.TemporaryDirectory(prefix='vr-native-arguments-') as directory:
            root=Path(directory);capture=root/'arguments.py';capture.write_text('import sys, json; print(json.dumps(sys.argv[1:]))',encoding='utf-8')
            quote=lambda path: "'"+str(path).replace("'","''")+"'"
            for enabled in (False,True):
                script=root/'forward.ps1';script.write_text('$ShiftedDrawProbe=$'+str(enabled).lower()+'\n'+assignment+'\n& '+quote(sys.executable)+' '+quote(capture)+' @shiftSelectionArgs\n',encoding='utf-8-sig')
                result=subprocess.run(['powershell','-NoProfile','-File',str(script)],capture_output=True,text=True,timeout=20)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertEqual(json.loads(result.stdout),['--lens-shift-selection'] if enabled else [])
    def test_view_shift_units_require_explicit_translated_probe(self):
        self.reject(['-ViewShiftUnits','.01'],'View shift units require the translated non-XR probe')
    def test_camera_turn_requires_private_root_diagnostic(self):
        self.reject(['-ProbeCameraTurn','Left'],'Camera turn requires the isolated root probe, private test save')
    def test_camera_turn_duration_requires_direction(self):
        self.reject(['-ProbeCameraTurnMilliseconds','150'],'Camera turn duration requires an explicit direction')
    def test_camera_turn_script_is_game_local_stick_input(self):
        path=str(ROOT/'vr/camera_probe_input.ps1').replace("'","''")
        for direction,button in (('Left','RLEFT'),('Right','RRIGHT')):
            command=". '"+path+"'; Get-VrProbeCameraTurnScript -Direction "+direction+" -Milliseconds 150"
            result=subprocess.run(['powershell','-NoProfile','-Command',command],capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout.strip(),'0:'+button+'/150')
    def test_root_probe_requires_isolated_baseline(self):
        self.reject(['-GpuRootAliasProbe'],'GPU root alias probe requires the isolated non-XR baseline')
    def test_root_probe_cannot_open_xr(self):
        self.reject(['-GpuRootAliasProbe','-IsolatedDrawProbe','-XrPreview'],'requires its own non-XR run')
    def test_root_probe_rejects_color_mode(self):
        self.reject(['-GpuRootAliasProbe','-IsolatedDrawProbe','-TranslatedColorDrawProbe'],'GPU root alias probe requires the translated depth diagnostic')
    def test_color_draw_requires_isolated_baseline(self):
        self.reject(['-TranslatedColorDrawProbe'],'Translated color probe requires the isolated non-XR baseline')
    def test_color_draw_cannot_open_xr(self):
        self.reject(['-TranslatedColorDrawProbe','-IsolatedDrawProbe','-XrPreview'],'requires its own non-XR run')
    def test_translated_draw_requires_isolated_baseline(self):
        self.reject(['-TranslatedDrawProbe'],'Translated draw probe requires the isolated non-XR baseline')
    def test_translated_draw_cannot_open_xr(self):
        self.reject(['-TranslatedDrawProbe','-IsolatedDrawProbe','-XrPreview'],'requires its own non-XR run')
    def test_shifted_draw_requires_isolated_baseline(self):
        self.reject(['-ShiftedDrawProbe'],'Lens-shift draw probe requires the isolated non-XR baseline')
    def test_shifted_draw_cannot_open_xr(self):
        self.reject(['-ShiftedDrawProbe','-IsolatedDrawProbe','-XrPreview'],'requires its own non-XR run')
    def test_gpu_matrix_probe_requires_isolated_baseline(self):
        self.reject(['-GpuEyeMatrixProbe'],'GPU matrix transport probe requires the isolated non-XR draw baseline')
    def test_gpu_matrix_probe_cannot_open_xr(self):
        self.reject(['-GpuEyeMatrixProbe','-IsolatedDrawProbe','-XrPreview'],'requires its own non-XR run')
    def test_hd_profile_selects_docked_output_mode(self):
        source=(ROOT/'vr/run-game.ps1').read_text(encoding='utf-8-sig')
        assignment=next(line for line in source.splitlines() if line.startswith('$env:HOS_HANDHELD ='))
        for hd,expected in ((False,'1'),(True,'0')):
            result=subprocess.run(['powershell','-NoProfile','-Command','$previewHD=$'+str(hd).lower()+'; '+assignment+'; Write-Output $env:HOS_HANDHELD'],capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(result.stdout.strip(),expected)
    def test_quality_options_require_isolated_immersive_mode(self):
        for args in (['-PreviewResolution','1080p'],['-FovMarginPercent','25'],['-LowLatencyPreview']):
            self.reject(args,'Preview quality and FOV margin require an isolated immersive mode')
    def test_private_capture_cannot_open_xr(self):
        self.reject(['-Seconds','180','-ImmersivePreview','-PreviewCaptureDirectory','unused'],'Private image capture requires the non-XR immersive readback test')
    def test_private_capture_cannot_escape_work(self):
        self.reject(['-Seconds','180','-ImmersiveReadbackTest','-PreviewCaptureDirectory',str(ROOT.parent/'outside-preview')],'Image captures require a fresh direct child of project/work')
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
