"""Authored matrices only; private real game captures are never test fixtures."""
import importlib.util
from pathlib import Path
import math
import unittest
import copy
spec = importlib.util.spec_from_file_location('audit', Path(__file__).resolve().parents[1] / 'audit_camera_uniforms.py')
audit = importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)


def sample(angle, x):
    c, s = math.cos(angle), math.sin(angle)
    view = [c, 0, -s, -x, 0, 1, 0, -2, s, 0, c, -3, 0, 0, 0, 1]
    projection = [1.1,0,0,0, 0,1.7,0,0, 0,0,-1.01,-.2, 0,0,-1,0]
    values = [0.0]*(0x230//4)
    values[:12] = view[:12]
    values[12:28] = audit.multiply(projection, view)
    values[28:44] = projection
    values[44:60] = audit.inverse(view)
    values[60:76] = audit.inverse(audit.multiply(projection, view))
    values[76:92] = audit.inverse(projection)
    return dict(schema=1, camera_revision=1, floats=values)

class CameraUniformTests(unittest.TestCase):
    def test_inverse_candidates_consistent_across_views(self):
        result = audit.audit([sample(.1,2), sample(.5,3), sample(.8,4)])
        self.assertIn('0xb0', result['common_algebraic_candidates']['inverse_view_4x4'])
        self.assertIn('0xf0', result['common_algebraic_candidates']['inverse_view_projection_4x4'])
        self.assertTrue(result['multiple_view_confirmation'])
        self.assertFalse(result['safe_to_patch'])
    def test_singular_and_corrupt_blocks_rejected(self):
        with self.assertRaises(ValueError): audit.inverse([0]*16)
        broken = sample(.2,1); broken['floats'][12] += 2
        with self.assertRaisesRegex(ValueError, 'Incoherent'): audit.audit([broken])
        broken = sample(.2,1); broken['floats'][0] = float('nan')
        with self.assertRaises(ValueError): audit.audit([broken])
    def test_repeated_view_is_not_multiple_view_proof(self):
        self.assertFalse(audit.audit([sample(.2,1)]*3)['multiple_view_confirmation'])
    def test_no_mutation(self):
        data = [sample(.2,1)];before=copy.deepcopy(data)
        audit.audit(data);self.assertEqual(data,before)

if __name__ == '__main__': unittest.main()
