"""Authored full camera words only; no game payload fixtures."""
import copy
import importlib.util
from pathlib import Path
import struct
import unittest

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
here=Path(__file__).resolve();audit=load('full_camera_audit',here.parents[1]/'audit_camera_payload.py')
fixture=load('full_camera_fixture',here.with_name('test_audit_camera_uniforms.py'))
def sample(angle,x):
    base=fixture.sample(angle,x)['floats'];values=[23.]*1024;values[:len(base)]=base
    view=values[:12]+[0,0,0,1];inverse=audit.matrices.inverse(view)
    values[256:259]=[inverse[3],inverse[7],inverse[11]]
    values[448:464]=audit.matrices.inverse(values[28:44])
    words=list(struct.unpack('<'+'I'*len(values),struct.pack('<'+'f'*len(values),*values)));words[-1]=0x7fc00001
    return dict(schema=1,kind='completed_gpu_camera_payload',serial=1,program=2,recording=9,guest=0x1000000000,pool_generation=3,capture_generation=4,width=1152,height=648,bytes=len(words)*4,words=words,**{k:True for k in audit.PROOFS})
class FullCameraTests(unittest.TestCase):
    def test_common_full_payload_candidates_across_views(self):
        result=audit.audit([sample(.1,2),sample(.5,3),sample(.8,4)])
        self.assertIn(1024,result['common_algebraic_candidates']['world_camera_position_xyz'])
        self.assertIn(1792,result['common_algebraic_candidates']['inverse_projection_4x4'])
        self.assertEqual(result['reference_distinct_values']['world_camera_position_xyz'],3)
        self.assertTrue(result['multiple_view_samples']);self.assertEqual(result['nonfinite_preserved_words'],3)
        self.assertFalse(result['safe_to_patch']);self.assertFalse(result['field_semantics_confirmed'])
    def test_duplicate_view_does_not_invent_confirmation(self):
        report=audit.audit([sample(.1,2)]*3);self.assertEqual(report['distinct_views'],1);self.assertFalse(report['multiple_view_samples'])
    def test_corrupt_prefix_and_bad_provenance_reject(self):
        for field in audit.PROOFS:
            value=sample(.1,2);value[field]=False
            with self.assertRaises(ValueError):audit.audit([value])
        value=sample(.1,2);value['words'][12]^=0x100000
        with self.assertRaises(ValueError):audit.audit([value])
    def test_bounds_and_integer_types_reject(self):
        for field,value in (('schema',True),('bytes',True),('recording',False),('width',0),('guest',0x11fffffff0)):
            v=sample(.1,2);v[field]=value
            with self.assertRaises(ValueError):audit.audit([v])
        for word in (True,-1,0x100000000):
            v=sample(.1,2);v['words'][-1]=word
            with self.assertRaises(ValueError):audit.audit([v])
    def test_mixed_extents_reject(self):
        first=sample(.1,2);second=sample(.5,3);second['words']+=[0]*4;second['bytes']+=16
        with self.assertRaises(ValueError):audit.audit([first,second])
    def test_capture_not_mutated(self):
        samples=[sample(.1,2)];before=copy.deepcopy(samples);audit.audit(samples);self.assertEqual(samples,before)
if __name__=='__main__':unittest.main()
