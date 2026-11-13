"""Complete stage/binding joins with authored modules and private temporary fixtures."""
import importlib.util
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
root=Path(__file__).resolve().parents[1]
audit=load("vr_program_test",root/"audit_graphics_programs.py")
fixture=load("vr_effect_fixture",Path(__file__).with_name("test_audit_graphics_effects.py"))
class ProgramTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix="vr-stage-inventory-");self.path=Path(self.tmp.name)
        self.vertex=self.write_module(fixture.module());self.fragment=self.write_module(fixture.module(model=4))
        self.record={"schema":1,"program":7,"stage_inventory_complete":True,"stages":[self.stage(self.vertex,0,1),self.stage(self.fragment,4,16)],
            "bindings":[{"module":self.vertex,"nvn_stage":0,"vk_stage":1,"binding":0,"array_element":0,"bank":3,"range_bytes":560,"primary_camera_binding":True}]}
    def tearDown(self):self.tmp.cleanup()
    def write_module(self,words):
        data=struct.pack("<"+"I"*len(words),*words);name=audit.fingerprint(data);(self.path/(name+".spv")).write_bytes(data);return name
    def stage(self,module,ns,vk):return {"module":module,"nvn_stage":ns,"vk_stage":vk,**{flag:False for flag in audit.WRITES}}
    def analyze(self,associated=False):
        data=json.dumps(self.record)+"\n";(self.path/"program-bindings.jsonl").write_text(data)
        return audit.analyze_directory(self.path,fixture.GRAMMAR,associated)
    def test_fragment_without_uniform_binding_is_still_checked(self):
        report=self.analyze();self.assertEqual(report["unique_stage_modules"],2);self.assertEqual(report["shader_subset_candidates"],1)
        self.assertFalse(report["safe_to_replay"]);self.assertFalse(report["pipeline_state_coverage_complete"])
    def test_camera_metadata_alone_does_not_invent_transform_reads(self):
        self.assertFalse(self.analyze()["programs"][0]["vertex_camera_transform_reads_observed"])
    def test_primary_binding_byte_load_is_observed(self):
        op=fixture.op;self.vertex=self.write_module(fixture.module([op(32,9,2,2),op(59,9,50,2),op(71,50,33,0),op(71,50,34,0),op(61,2,51,50)]))
        self.record["stages"][0]["module"]=self.vertex;self.record["bindings"][0]["module"]=self.vertex
        report=self.analyze();self.assertTrue(report["programs"][0]["vertex_camera_transform_reads_observed"])
        self.assertEqual(report["shader_candidates_with_vertex_transform_reads"],1)
    def test_missing_unbound_fragment_cannot_be_called_complete(self):
        (self.path/(self.fragment+".spv")).unlink();report=self.analyze()
        self.assertEqual(report["uniform_binding_summary"]["missing_observed_modules"],0)
        self.assertEqual(report["missing_stage_modules"],1);self.assertEqual(report["shader_subset_candidates"],0)
    def test_legacy_binding_record_is_not_full_stage_evidence(self):
        self.record.pop("stages");self.record.pop("stage_inventory_complete");report=self.analyze()
        self.assertEqual(report["complete_stage_inventories"],0);self.assertEqual(report["shader_subset_candidates"],0)
    def test_null_stage_module_is_explicitly_unavailable(self):
        self.record["stages"][1]["module"]=None;self.assertEqual(self.analyze()["shader_subset_candidates"],0)
    def test_native_effect_hint_excludes_candidate(self):
        for flag in audit.WRITES:
            self.record["stages"][0][flag]=True;self.assertEqual(self.analyze()["shader_subset_candidates"],0);self.record["stages"][0][flag]=False
    def test_binding_stage_mismatch_rejected(self):
        self.record["bindings"][0]["module"]=self.fragment
        with self.assertRaises(ValueError):self.analyze()
    def test_content_fingerprint_mismatch_rejected(self):
        path=self.path/(self.fragment+".spv");path.write_bytes(path.read_bytes()+b"\0\0\0\0")
        with self.assertRaises(ValueError):self.analyze()
    def test_duplicate_and_oversized_stage_inventory_rejected(self):
        self.record["stages"].append(self.record["stages"][0])
        with self.assertRaises(ValueError):self.analyze()
    def test_untyped_effect_flag_rejected(self):
        self.record["stages"][0]["uses_global_memory"]=0
        with self.assertRaises(ValueError):self.analyze()
    def test_mismatched_module_entry_excludes_program(self):
        self.record["stages"][1]["module"]=self.vertex;self.assertEqual(self.analyze()["shader_subset_candidates"],0)
    def test_required_association_cannot_be_missing(self):
        with self.assertRaises((ValueError,OSError)):self.analyze(True)
    def test_association_hashes_bind_capture_inputs(self):
        self.analyze();graph=b"{}";(self.path/"image-lineage.json").write_bytes(graph)
        manifest={"schema":1,"producer":"vr/run-game.ps1","same_process_capture":True,
            "lineage_sha256":hashlib.sha256(graph).hexdigest(),"bindings_sha256":hashlib.sha256((self.path/"program-bindings.jsonl").read_bytes()).hexdigest()}
        (self.path/"capture-association.json").write_text(json.dumps(manifest));self.assertTrue(self.analyze(True)["same_process_manifest_verified"])
        manifest["bindings_sha256"]="0"*64;(self.path/"capture-association.json").write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):self.analyze(True)
if __name__=="__main__":unittest.main()
