# SPDX-License-Identifier: GPL-3.0-or-later
"""Authored resource graphs only; no game, shader or GPU runtime."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('stereo_candidates',ROOT/'vr/analyze_stereo_candidates.py');planner=importlib.util.module_from_spec(spec);spec.loader.exec_module(planner)
def event(kind,program,reads,writes,buffer_reads=None,buffer_writes=None):
    return dict(kind=kind,program=program,reads=reads,writes=writes,buffer_reads=buffer_reads or [],buffer_writes=buffer_writes or [],seed=False,dependent=False,opaque_buffers=False)
def capture():
    return dict(schema=1,coverage_complete=False,replay_ready=False,present_image=14,events=[
        event(1,7,[],[],buffer_writes=[[500,0,16,1]]),
        event(0,8,[],[[14,1]],buffer_reads=[[500,0,16,1]]),
        event(1,9,[],[[99,1]])])
class StereoCandidateTests(unittest.TestCase):
    def test_camera_compute_promotes_its_buffer_and_consumers(self):
        data=capture();before=copy.deepcopy(data);result=planner.analyze(data,{7})
        self.assertEqual(result['baseline']['private_buffer_alias_groups'],0)
        self.assertEqual(result['conservative_candidate']['private_buffer_alias_groups'],1)
        self.assertEqual(result['conservative_candidate']['private_image_candidates'],1)
        self.assertEqual(result['added_seed_events_in_present_slice'],1)
        self.assertEqual(result['added_dependent_events_in_present_slice'],2)
        self.assertEqual(data,before);self.assertFalse(result['replay_ready']);self.assertFalse(result['per_dispatch_camera_semantics_confirmed'])
    def test_only_compute_and_present_dependencies_are_promoted(self):
        result=planner.analyze(capture(),{8,9,10})
        self.assertEqual(result['added_dependent_events_in_present_slice'],0)
        self.assertEqual(result['programs_seen_in_frame'],1)
        self.assertEqual(result['programs_not_seen_in_frame'],2)
    def test_future_versions_and_bad_program_ids_rejected(self):
        data=capture();data['events'][1]['buffer_reads']=[[500,0,16,2]]
        with self.assertRaises(ValueError):planner.analyze(data,{7})
        for programs in ({True},{0},range(1,66)):
            with self.assertRaises(ValueError):planner.analyze(capture(),programs)
    def test_same_process_association_requires_both_hashes(self):
        work=ROOT/'work';work.mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='stereo-association-',dir=work) as name:
            root=Path(name).resolve();self.assertTrue(root.is_relative_to(work.resolve()))
            graph=json.dumps(capture()).encode();metadata=b'authored metadata, not a module\n'
            (root/'image-lineage.json').write_bytes(graph);(root/'program-bindings.jsonl').write_bytes(metadata)
            record=dict(schema=1,producer='vr/run-game.ps1',same_process_capture=True,lineage_sha256=hashlib.sha256(graph).hexdigest(),bindings_sha256=hashlib.sha256(metadata).hexdigest())
            (root/'capture-association.json').write_text(json.dumps(record));self.assertEqual(planner.association(root),root)
            (root/'program-bindings.jsonl').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'hash'):planner.association(root)
if __name__=='__main__':unittest.main()
