"""Invented moving matrices and SPIR-V fragments, no game dump required."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

audit=load('consumers',ROOT/'vr/audit_camera_consumers.py')
matrices=load('history_fixture',ROOT/'vr/tests/test_camera_history_audit.py')
spirv=load('shader_fixture',ROOT/'vr/tests/test_audit_spirv_uniforms.py')


class Consumers(unittest.TestCase):
    def run_fixture(self,d,offset=112,dynamic=False,primary=True):
        root=Path(d);hist=root/'history';shader=root/'shaders';hist.mkdir();shader.mkdir();matrices.fixture(hist)
        words=spirv.module(dynamic=dynamic)
        at=5
        while at<len(words):
            size,code=words[at]>>16,words[at]&65535
            if code==43 and words[at+2]==4:words[at+3]=offset//16
            if code==43 and words[at+2]==5:words[at+3]=1024
            at+=size
        (shader/'0000000000000001.spv').write_bytes(struct.pack('<'+'I'*len(words),*words))
        binding=dict(module='0000000000000001',nvn_stage=0,vk_stage=1,binding=6,array_element=0,bank=3,range_bytes=0xb00,primary_camera_binding=primary)
        (shader/'program-bindings.jsonl').write_text(json.dumps(dict(schema=1,program=2,bindings=[binding]))+'\n')
        return audit.analyze(hist,shader)
    def test_projection_read_does_not_resolve_invariant_time_role(self):
        with tempfile.TemporaryDirectory() as d:result=self.run_fixture(d)
        self.assertTrue(result['captured_program_observed']);region=result['consumers'][0]['numeric_regions_read'][0]
        self.assertEqual(region['matrix_kind'],'projection_4x4');self.assertIsNone(region['unique_sample_lag'])
        self.assertFalse(result['safe_to_patch']);self.assertFalse(result['complete_shader_semantic_proof'])
    def test_prior_view_footprint_is_a_numeric_lag_candidate(self):
        with tempfile.TemporaryDirectory() as d:result=self.run_fixture(d,0xa10)
        regions=result['consumers'][0]['numeric_regions_read'];self.assertEqual(len(regions),1)
        self.assertEqual(regions[0]['matrix_kind'],'view_3x4');self.assertEqual(regions[0]['unique_sample_lag'],1)
        self.assertEqual(regions[0]['static_read_word_count'],4);self.assertEqual(regions[0]['candidate_word_count'],12)
    def test_nonprimary_binding_cannot_invent_camera_consumption(self):
        with tempfile.TemporaryDirectory() as d:result=self.run_fixture(d,primary=False)
        self.assertEqual(result['consumers'],[])
    def test_dynamic_reads_remain_unresolved(self):
        with tempfile.TemporaryDirectory() as d:result=self.run_fixture(d,dynamic=True)
        self.assertEqual(result['unresolved_primary_queries'],1);self.assertEqual(result['consumers'][0]['numeric_regions_read'],[])


if __name__=='__main__':unittest.main()
