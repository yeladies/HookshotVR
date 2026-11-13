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
    def test_eye_patch_reports_only_transport_coverage(self):
        binding=self.record["bindings"][0]
        result=audit.eye_patch_footprint([(binding,{"static_subset_resolved":True,"static_read_word_offsets":[0,48,112,176,220]})])
        self.assertEqual(result["status"],"matrix_footprint_only")
        self.assertFalse(result["safe_to_patch"]);self.assertFalse(result["shader_semantics_confirmed"])
    def test_preserved_fields_are_not_assumed_eye_independent(self):
        result=audit.eye_patch_footprint([(self.record["bindings"][0],{"static_subset_resolved":True,"static_read_word_offsets":[0,224,448,508]})])
        self.assertEqual(result["status"],"unreviewed_preserved_fields")
        self.assertEqual(result["preserved_read_byte_offsets_requiring_review"],[224,448,508])
    def test_unresolved_camera_pointer_blocks_footprint_completeness(self):
        for query in ({},{"static_subset_resolved":False,"static_read_word_offsets":[0]}):
            result=audit.eye_patch_footprint([(self.record["bindings"][0],query)])
            self.assertEqual(result["status"],"unresolved_camera_reads")
            self.assertFalse(result["all_primary_footprints_resolved"])
    def test_eye_patch_keeps_stage_and_array_identity(self):
        first=self.record["bindings"][0];second={**first,"vk_stage":16,"array_element":1}
        query={"static_subset_resolved":True,"static_read_word_offsets":[48]}
        result=audit.eye_patch_footprint([(first,query),(second,query)])
        self.assertEqual([r["vk_stage"] for r in result["primary_binding_footprints"]],[1,16])
        self.assertEqual(result["primary_binding_footprints"][1]["array_element"],1)
    def test_eye_patch_rejects_malformed_offsets(self):
        for words in ([True],[-4],[1],[560],[0]*16385):
            with self.assertRaises(ValueError):
                audit.eye_patch_footprint([(self.record["bindings"][0],{"static_read_word_offsets":words})])
    def test_eye_patch_no_binding_or_no_reads_is_not_matrix_evidence(self):
        self.assertEqual(audit.eye_patch_footprint([])["status"],"unresolved_camera_reads")
        result=audit.eye_patch_footprint([(self.record["bindings"][0],{"static_subset_resolved":True,"static_read_word_offsets":[]})])
        self.assertEqual(result["status"],"no_observed_matrix_reads")
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
    def test_lens_selection_requires_resolved_vertex_matrix_prefix(self):
        op=fixture.op;self.vertex=self.write_module(fixture.module([op(32,9,2,2),op(59,9,50,2),op(71,50,33,0),op(71,50,34,0),op(61,2,51,50)]))
        self.record["stages"][0]["module"]=self.vertex;self.record["bindings"][0]["module"]=self.vertex
        report=self.analyze();text=audit.baseline_selection_text(report,"a"*32,"1234abcd",True)
        self.assertTrue(text.startswith("HSVR_LENS_SHIFT_DRAW_V1\n"))
        program=report["programs"][0];program["eye_patch_preflight"]["status"]="unreviewed_preserved_fields"
        with self.assertRaises(ValueError):audit.baseline_selection_text(report,"a"*32,"1234abcd",True)
    def test_lens_selection_rejects_primary_fragment_matrix_reads(self):
        binding=self.record["bindings"][0]
        program={"program":7,"shader_subset_candidate":True,"vertex_camera_transform_reads_observed":True,
            "eye_patch_preflight":audit.eye_patch_footprint([({**binding,"vk_stage":16},{"static_subset_resolved":True,"static_read_word_offsets":[0]})])}
        with self.assertRaises(ValueError):audit.baseline_selection_text({"safe_to_replay":False,"programs":[program]},"a"*32,"1234abcd",True)
    def test_baseline_selection_requires_actual_vertex_transform_reads(self):
        with self.assertRaises(ValueError):audit.baseline_selection_text(self.analyze(),"a"*32,"1234abcd")
    def test_baseline_selection_is_bounded_and_run_scoped(self):
        op=fixture.op;self.vertex=self.write_module(fixture.module([op(32,9,2,2),op(59,9,50,2),op(71,50,33,0),op(71,50,34,0),op(61,2,51,50)]))
        self.record["stages"][0]["module"]=self.vertex;self.record["bindings"][0]["module"]=self.vertex
        report=self.analyze();text=audit.baseline_selection_text(report,"a"*32,"1234abcd")
        self.assertTrue(text.startswith("HSVR_BASELINE_DRAW_V1\n"+"a"*32+" 1234abcd 1\n0000000000000007 2\n"))
        self.assertLessEqual(len(text),8192)
        with self.assertRaises(ValueError):audit.baseline_selection_text(report,"wrong","1234abcd")
        report["programs"][0]["stages"][0]["effects"]["supported_read_only_subset"]=False
        with self.assertRaises(ValueError):audit.baseline_selection_text(report,"a"*32,"1234abcd")
    def preflight_fixture(self):
        self.row={"schema":1,"program":"7","recording":9,"frame":1,"width":64,"height":64,"uniform_bytes":560,"camera_bindings":1,"read_images":3,
            "reason":"live_state_candidate_requires_shader_review","extra_draw_recorded":False,"shader_effects_reviewed":False,"safe_to_replay":False}
        self.programs=[{"program":7,"shader_subset_candidate":True,"vertex_camera_transform_reads_observed":True}]
        self.write_preflight()
    def write_preflight(self):
        data=(json.dumps(self.row)+"\n").encode();(self.path/"baseline-preflight.jsonl").write_bytes(data)
        (self.path/"capture-association.json").write_text(json.dumps({"same_process_capture":True,"baseline_preflight_sha256":hashlib.sha256(data).hexdigest()}))
    def test_preflight_joins_only_shader_and_live_candidates(self):
        self.preflight_fixture();report=audit.baseline_preflight(self.path,self.programs)
        self.assertEqual(report["shader_and_live_state_candidates"],[7]);self.assertFalse(report["safe_to_replay"])
        self.programs[0]["vertex_camera_transform_reads_observed"]=False
        self.assertEqual(audit.baseline_preflight(self.path,self.programs)["shader_and_live_state_candidates"],[])
    def test_preflight_unknown_program_or_rejection_cannot_select(self):
        self.preflight_fixture();self.assertEqual(audit.baseline_preflight(self.path,[])["shader_and_live_state_candidates"],[])
        self.row["reason"]="camera_source_not_current_mirror";self.write_preflight()
        self.assertEqual(audit.baseline_preflight(self.path,self.programs)["shader_and_live_state_candidates"],[])
    def test_preflight_accepts_windows_hash_case_but_not_wrong_hash_type(self):
        self.preflight_fixture();path=self.path/"capture-association.json";manifest=json.loads(path.read_text())
        manifest["baseline_preflight_sha256"]=manifest["baseline_preflight_sha256"].upper();path.write_text(json.dumps(manifest))
        self.assertEqual(audit.baseline_preflight(self.path,self.programs)["shader_and_live_state_candidates"],[7])
        manifest["baseline_preflight_sha256"]=123;path.write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):audit.baseline_preflight(self.path,self.programs)
    def test_preflight_stale_or_mutated_capture_rejected(self):
        self.preflight_fixture();(self.path/"baseline-preflight.jsonl").write_text("{}")
        with self.assertRaises(ValueError):audit.baseline_preflight(self.path,self.programs)
    def test_preflight_requires_diagnostic_flags_and_typed_bounds(self):
        self.preflight_fixture()
        for field,value in (("extra_draw_recorded",True),("safe_to_replay",True),("shader_effects_reviewed",True),("width",True),("uniform_bytes",65540),("camera_bindings",0)):
            original=self.row[field];self.row[field]=value;self.write_preflight()
            with self.assertRaises(ValueError):audit.baseline_preflight(self.path,self.programs)
            self.row[field]=original
    def test_preflight_optional_for_legacy_capture(self):
        self.assertIsNone(audit.baseline_preflight(self.path,[]))
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
