"""Invented moving matrices and buffers; no game data required."""
import importlib.util
import json
import math
import struct
import tempfile
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('camera_history_audit',Path(__file__).parents[1]/'audit_camera_history.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)


def view(angle):
    c,s=math.cos(angle),math.sin(angle)
    return [c,0,-s,-2*c+4*s,0,1,0,-3,s,0,c,-2*s-4*c,0,0,0,1]


def fixture(root,moving=True):
    rows=[];p=[1.1,0,.1,0,0,1.4,-.05,0,0,0,-1.0002,-.20002,0,0,-1,0]
    for k in range(32):
        v=view(.03*k if moving else 0);previous=view(.03*(k-1) if moving else 0);block=[0.0]*(0xb00//4)
        def put(at,values):block[at//4:at//4+len(values)]=values
        put(0,v[:12]);put(0x30,audit.helpers.multiply(p,v));put(0x70,p);put(0xb0,audit.helpers.inverse(v)[:12])
        put(0xa10,previous[:12]);put(0xa40,audit.helpers.inverse(previous)[:12]);put(0xa70,p);put(0xab0,audit.helpers.inverse(p))
        block[500]=float('nan') # Non-matrix payload bytes need not encode finite floats.
        name=f'frame-{1000+k}.bin';(root/name).write_bytes(struct.pack('<'+'f'*len(block),*block))
        rows.append({'schema':1,'frame':1000+k,'program':2,'recording':100+k,'camera_revision':200+k,'source':0x1000+k*0x1000,
                     'width':1280,'height':720,'bytes':len(block)*4,'file':name,'scene_camera_match':True,'submission_overlay_copy':True,'gpu_completed_readback':False})
    (root/'index.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows));return rows


class History(unittest.TestCase):
    def test_current_and_lagged_matrices(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);fixture(root);result=audit.analyze(root);views={r['offset_bytes']:r for r in result['numeric_matrix_candidates']['view_3x4']}
            self.assertEqual(views[0]['unique_sample_lag'],0);self.assertEqual(views[0xa10]['unique_sample_lag'],1)
            p={r['offset_bytes']:r for r in result['numeric_matrix_candidates']['projection_4x4']}
            self.assertIsNone(p[0xa70]['unique_sample_lag']);self.assertFalse(result['safe_to_patch']);self.assertFalse(result['gpu_completed_readback'])
    def test_source_submit_metadata_is_only_observation_evidence(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);rows=fixture(root)
            for k,row in enumerate(rows):row.update(source_nvn_submit=500+k,observation_game_present=700+k,
                observation_present_ambiguous=False,primary_sequence=900+k,producer_present_verified=False)
            rows[0].update(observation_present_ambiguous=True,observation_game_present=-1)
            (root/'index.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows))
            result=audit.analyze(root)
            self.assertEqual(result['observation_tagged_samples'],32);self.assertEqual(result['ambiguous_observation_samples'],1)
            self.assertEqual(result['source_nvn_submit_count'],32);self.assertEqual(result['recording_differs_from_source_samples'],32)
            self.assertFalse(result['producer_present_verified']);self.assertFalse(result['safe_to_patch'])
    def test_partial_or_invalid_source_metadata_rejected(self):
        for change in ('partial','mixed','negative','ambiguous','producer','sequence','bool_submit'):
            with self.subTest(change=change),tempfile.TemporaryDirectory() as d:
                root=Path(d);rows=fixture(root)
                for k,row in enumerate(rows):row.update(source_nvn_submit=500+k,observation_game_present=700+k,
                    observation_present_ambiguous=False,primary_sequence=900+k,producer_present_verified=False)
                if change=='partial':del rows[0]['primary_sequence']
                if change=='mixed':
                    for field in ('source_nvn_submit','observation_game_present','observation_present_ambiguous','primary_sequence','producer_present_verified'):del rows[0][field]
                if change=='negative':rows[0]['observation_game_present']=-1
                if change=='ambiguous':rows[0]['observation_present_ambiguous']=True
                if change=='producer':rows[0]['producer_present_verified']=True
                if change=='sequence':rows[0]['primary_sequence']=0
                if change=='bool_submit':rows[0]['source_nvn_submit']=True
                (root/'index.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows))
                with self.assertRaises(ValueError):audit.analyze(root)
    def test_stationary_capture_is_not_temporal_proof(self):
        with tempfile.TemporaryDirectory() as d:
            fixture(Path(d),False)
            with self.assertRaises(ValueError):audit.analyze(d)
    def test_provenance_path_and_prefix_corruption(self):
        for change in ('provenance','path','prefix','frames'):
            with self.subTest(change=change),tempfile.TemporaryDirectory() as d:
                root=Path(d);rows=fixture(root)
                if change=='provenance':rows[0]['gpu_completed_readback']=True
                if change=='path':rows[0]['file']='../outside.bin'
                if change=='frames':rows[1]['frame']=rows[0]['frame']
                if change=='prefix':
                    path=root/rows[0]['file'];data=bytearray(path.read_bytes());struct.pack_into('<f',data,12,999);path.write_bytes(data)
                (root/'index.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows))
                with self.assertRaises(ValueError):audit.analyze(root)


if __name__=='__main__':unittest.main()
