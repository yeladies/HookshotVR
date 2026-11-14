"""Authored physical-read inventory fixtures; never game shader input."""
import importlib.util
import json
from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[1]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
fixture=load('root_effect_program_fixture',Path(__file__).with_name('test_audit_graphics_programs.py'))
audit=load('root_read_inventory',root/'audit_private_root_reads.py')

class RootReadInventoryTests(unittest.TestCase):
    setUp=fixture.ProgramTests.setUp
    tearDown=fixture.ProgramTests.tearDown
    write_module=fixture.ProgramTests.write_module
    stage=fixture.ProgramTests.stage
    analyze=fixture.ProgramTests.analyze
    def inventory(self):
        (self.path/'program-bindings.jsonl').write_text(json.dumps(self.record)+'\n')
        return audit.analyze_directory(self.path,fixture.fixture.GRAMMAR,require_association=False)
    def global_fixture(self):
        op=fixture.fixture.op
        self.vertex=self.write_module(fixture.fixture.module([op(120,6,30,8),op(61,2,31,30)],memory=(5348,1)))
        self.record['stages'][0]['module']=self.vertex;self.record['bindings'][0]['module']=self.vertex
        for stage in self.record['stages']:stage['stores_global_memory']=False
        self.record['stages'][0]['uses_global_memory']=True
    def test_explicit_readonly_globals_are_only_inventory_candidates(self):
        self.global_fixture();report=self.inventory();self.assertEqual(report['global_read_effect_candidates'],1)
        row=report['programs'][0];self.assertFalse(row['private_root_draw_ready']);self.assertFalse(row['safe_to_replay'])
        self.assertFalse(row['vertex_camera_transform_reads_observed'])
        # Existing diagnostic selection cannot accidentally use the relaxed inventory.
        self.assertEqual(self.analyze()['shader_subset_candidates'],0)
    def test_missing_metadata_is_not_assumed_readonly(self):
        self.global_fixture();del self.record['stages'][0]['stores_global_memory'];report=self.inventory()
        self.assertEqual(report['global_programs_missing_write_metadata'],1);self.assertEqual(report['global_read_effect_candidates'],0)
    def test_writes_from_either_source_reject(self):
        for source in ('native','spirv'):
            self.global_fixture()
            if source=='native':self.record['stages'][0]['stores_global_memory']=True
            else:
                op=fixture.fixture.op;self.vertex=self.write_module(fixture.fixture.module([op(120,6,30,8),op(62,30,8)],memory=(5348,1)))
                self.record['stages'][0]['module']=self.vertex;self.record['bindings'][0]['module']=self.vertex
            self.assertEqual(self.inventory()['global_read_effect_candidates'],0)
    def test_malformed_metadata_rejected(self):
        self.global_fixture();self.record['stages'][0]['stores_global_memory']=0
        with self.assertRaises(ValueError):self.inventory()
    def test_no_global_reads_does_not_invent_a_candidate(self):
        for stage in self.record['stages']:stage['stores_global_memory']=False
        self.assertEqual(self.inventory()['global_read_effect_candidates'],0)
    def test_missing_module_does_not_become_a_candidate(self):
        self.global_fixture();(self.path/(self.vertex+'.spv')).unlink()
        self.assertEqual(self.inventory()['global_read_effect_candidates'],0)

if __name__=='__main__':unittest.main()
