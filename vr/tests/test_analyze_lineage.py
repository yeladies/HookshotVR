"""Authored image versions only; no game data or graphics runtime."""
import importlib.util
from pathlib import Path
import unittest
import copy

spec = importlib.util.spec_from_file_location('lineage', Path(__file__).resolve().parents[1] / 'analyze_lineage.py')
lineage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lineage)

def event(kind, reads, writes, seed=False, dependent=False, opaque=False):
    return dict(kind=kind, reads=reads, writes=writes, seed=seed, dependent=dependent, opaque_buffers=opaque)

class LineageTests(unittest.TestCase):
    def capture(self):
        return dict(schema=1, replay_ready=False, coverage_complete=False, present_image=14, events=[
            event(1, [[101, 0]], [[10, 1]]),
            event(0, [[10, 1]], [[11, 1]], seed=True, dependent=True),
            event(1, [[11, 1]], [[12, 1]], dependent=True, opaque=True),
            event(2, [[12, 1]], [[14, 1]], dependent=True),
            event(0, [[200, 0], [14, 1]], [[14, 2]], dependent=True),
            event(1, [], [[99, 1]])])
    def test_present_slice_excludes_unrelated_work(self):
        result = lineage.analyze(self.capture())
        self.assertEqual(result['present_slice_events'], 5)
        self.assertEqual(result['camera_seed_events_in_slice'], 1)
        self.assertEqual(result['opaque_buffer_events_in_slice'], 1)
        self.assertEqual(result['imported_image_roots_in_slice'], 2)
        self.assertEqual(result['private_image_candidates'], 3)
        self.assertEqual(result['per_eye_work_candidates'], dict(graphics=2, compute=1, replacement=1))
        self.assertFalse(result['replay_ready'])
    def test_future_version_is_rejected(self):
        capture = self.capture(); capture['events'][0]['reads'] = [[14, 2]]
        with self.assertRaisesRegex(ValueError, 'future'):
            lineage.analyze(capture)
    def test_duplicate_and_skipped_versions_are_rejected(self):
        for raw in ([[14, 1]], [[14, 3]]):
            capture = self.capture(); capture['events'][4]['writes'] = raw
            with self.assertRaises(ValueError): lineage.analyze(capture)
    def test_missing_producer_and_claimed_completeness_rejected(self):
        capture = self.capture(); capture['present_image'] = 900
        with self.assertRaises(ValueError): lineage.analyze(capture)
        capture = self.capture(); capture['coverage_complete'] = True
        with self.assertRaises(ValueError): lineage.analyze(capture)
    def test_buffer_producer_is_in_present_slice(self):
        producer = event(1, [], [])
        producer['buffer_writes'] = [[500, 0, 16, 1]]
        consumer = event(0, [], [[14, 1]], seed=True, dependent=True)
        consumer['buffer_reads'] = [[500, 0, 16, 1], [500, 8, 4, 0]]
        capture = dict(schema=1, replay_ready=False, coverage_complete=False, present_image=14, events=[producer, consumer])
        result = lineage.analyze(capture)
        self.assertEqual(result['present_slice_events'], 2)
        self.assertEqual(result['observed_buffer_writes'], 1)
        self.assertEqual(result['imported_buffer_ranges_in_slice'], 1)
        consumer['buffer_reads'][0][0] = 999
        with self.assertRaisesRegex(ValueError, 'buffer version'): lineage.analyze(capture)
    def test_buffer_alias_closure_preserves_overlap(self):
        writes = {(100, 8, 4), (100, 18, 2)}
        reads = {(100, 0, 16), (100, 12, 12), (100, 64, 16), (200, 0, 99)}
        result = lineage.buffer_alias_closure(writes, reads)
        self.assertEqual(result, [(100, 0, 24)])
        self.assertEqual(writes, {(100, 8, 4), (100, 18, 2)})
        self.assertEqual(lineage.buffer_alias_closure(set(), reads), [])
    def test_image_inventory_budget_is_reference_only(self):
        capture = self.capture()
        capture['resources'] = [dict(image=image, allocation_bytes=4096, format=37, width=32, height=32,
                                     depth=1, levels=1, layers=1, samples=1, aspect=1, type=1, sparse=False)
                                for image in (11, 12, 14)]
        result = lineage.analyze(capture)
        self.assertEqual(result['missing_private_image_descriptions'], 0)
        self.assertEqual(result['private_image_allocation_reference_bytes_two_eyes'], 24576)
        self.assertFalse(result['replay_ready'])
        capture['resources'].append(capture['resources'][0])
        with self.assertRaises(ValueError): lineage.analyze(capture)
    def test_input_not_modified(self):
        capture = self.capture(); original = copy.deepcopy(capture)
        lineage.analyze(capture); self.assertEqual(capture, original)



class SameStatePlanTests(unittest.TestCase):
    def analyze(self, events):
        return lineage.analyze(dict(schema=1, replay_ready=False, coverage_complete=False,
                                    present_image=14, events=events))['same_state_plan']
    def test_orders_full_chain_and_keeps_shared_work_once(self):
        result=self.analyze(LineageTests().capture()['events'])
        self.assertEqual(result['shared_work_review_indices'], [0])
        self.assertEqual(result['per_eye_work_review_indices'], [1,2,3,4])
        self.assertEqual(result['per_eye_image_ids'], [11,12,14])
        self.assertEqual(result['work'][-1]['parents'], [3])
        self.assertEqual(result['simulation_updates_per_pair'], 1)
        self.assertTrue(result['atomic_pair_submission_required'])
        self.assertFalse(result['replay_ready'])
    def test_recomputes_flags_instead_of_trusting_capture(self):
        events=LineageTests().capture()['events'];events[2]['dependent']=False
        result=self.analyze(events)
        self.assertEqual(result['dependency_flag_mismatch_indices'], [2])
        self.assertIn(2,result['per_eye_work_review_indices'])
        self.assertFalse(result['replay_ready'])
    def test_shared_buffer_producer_and_eye_buffer_consumer(self):
        producer=event(1,[],[]);producer['buffer_writes']=[[5,0,16,1]]
        seed=event(1,[],[],seed=True,dependent=True)
        seed['buffer_reads']=[[5,0,16,1]];seed['buffer_writes']=[[6,0,16,2]]
        consumer=event(0,[],[[14,1]],dependent=True)
        consumer['buffer_reads']=[[6,0,16,2]]
        result=self.analyze([producer,seed,consumer])
        self.assertEqual(result['shared_work_review_indices'],[0])
        self.assertEqual(result['per_eye_work_review_indices'],[1,2])
        self.assertEqual(result['per_eye_buffer_ranges'],[(6,0,16)])
    def test_imported_history_never_becomes_proven_shared(self):
        result=self.analyze([event(0,[[99,0]],[[14,1]],seed=True,dependent=True)])
        self.assertEqual(result['imported_image_review_ids'],[99])
        self.assertTrue(any('imported resources' in s for s in result['blockers']))
        self.assertFalse(result['replay_ready'])
    def test_overlay_dependency_is_not_a_world_or_hud_identity(self):
        result=self.analyze(LineageTests().capture()['events'])
        self.assertEqual(result['unseeded_graphics_review_indices'],[4])
        self.assertFalse(result['hud_separation_verified'])
    def test_no_camera_seed_never_claims_stereo(self):
        result=self.analyze([event(0,[],[[14,1]])])
        self.assertEqual(result['per_eye_work_review_indices'],[])
        self.assertFalse(result['replay_ready'])

class TransferLineageTests(unittest.TestCase):
    def test_transfer_and_partial_clear_keep_source_chain(self):
        events=[event(0,[],[[1,1]],seed=True,dependent=True),event(3,[[1,1]],[[2,1]],dependent=True),event(4,[[2,1]],[[2,2]],dependent=True)]
        result=lineage.analyze(dict(schema=1,replay_ready=False,coverage_complete=False,present_image=2,events=events))
        self.assertEqual(result['present_slice_kinds']['transfer'],1)
        self.assertEqual(result['per_eye_work_candidates']['clear'],1)
        self.assertEqual(result['same_state_plan']['per_eye_work_review_indices'],[0,1,2])
        self.assertFalse(result['replay_ready'])
    def test_boolean_kind_is_not_a_work_type(self):
        capture=LineageTests().capture();capture['events'][0]['kind']=True
        with self.assertRaises(ValueError):lineage.analyze(capture)

if __name__ == '__main__': unittest.main()
