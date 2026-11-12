"""Authored SPIR-V and binding JSON only, stored temporarily under ignored work/."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
batch=load('batch',ROOT/'vr/audit_shader_bindings.py')
fixture=load('fixture',ROOT/'vr/tests/test_audit_spirv_uniforms.py')

class ShaderBindingTests(unittest.TestCase):
    def setUp(self):
        work=(ROOT/'work').resolve();work.mkdir(exist_ok=True)
        self.temp=tempfile.TemporaryDirectory(prefix='shader-bindings-',dir=work)
        self.root=Path(self.temp.name).resolve();self.assertTrue(self.root.is_relative_to(work))
        self.addCleanup(self.temp.cleanup)
        words=fixture.module();(self.root/'0000000000000001.spv').write_bytes(struct.pack('<'+'I'*len(words),*words))
        self.binding=dict(module='0000000000000001',nvn_stage=0,vk_stage=1,binding=6,array_element=0,bank=3,range_bytes=560,primary_camera_binding=True)
    def write(self,binding):
        (self.root/'program-bindings.jsonl').write_text(json.dumps(dict(schema=1,program=7,bindings=[binding]))+'\n',encoding='utf-8')
    def test_primary_binding_is_resolved_without_shader_text(self):
        self.write(self.binding);result=batch.analyze_directory(self.root)
        self.assertEqual(result['queries_with_static_reads'],1)
        self.assertEqual(result['camera_regions_by_nvn_stage'],{'0':['projection']})
        self.assertFalse(result['safe_to_patch'])
    def test_nonprimary_binding_is_not_assumed_to_be_camera(self):
        self.binding['primary_camera_binding']=False;self.write(self.binding)
        self.assertEqual(batch.analyze_directory(self.root)['primary_camera_bindings'],0)
    def test_missing_and_escaping_modules_are_explicit(self):
        self.binding['module']=None;self.write(self.binding)
        self.assertEqual(batch.analyze_directory(self.root)['missing_primary_modules'],1)
        self.binding['module']='../secret';self.write(self.binding)
        with self.assertRaises(ValueError):batch.analyze_directory(self.root)
    def test_dynamic_access_remains_unresolved(self):
        words=fixture.module(dynamic=True);(self.root/'0000000000000001.spv').write_bytes(struct.pack('<'+'I'*len(words),*words))
        self.write(self.binding);result=batch.analyze_directory(self.root)
        self.assertEqual(result['unresolved_queries'],1)
        self.assertEqual(result['queries_with_static_reads'],0)

if __name__=='__main__':unittest.main()
