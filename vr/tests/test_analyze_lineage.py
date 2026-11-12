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

if __name__ == '__main__': unittest.main()
