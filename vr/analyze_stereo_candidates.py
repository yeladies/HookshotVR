# SPDX-License-Identifier: GPL-3.0-or-later
"""Conservatively extend a private lineage plan with observed compute camera candidates."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
lineage=load('stereo_lineage',Path(__file__).with_name('analyze_lineage.py'))
bindings=load('stereo_bindings',Path(__file__).with_name('audit_shader_bindings.py'))

def analyze(data,programs):
    programs=set(programs)
    if len(programs)>64 or any(type(p) is not int or not 0<p<(1<<64) for p in programs):
        raise ValueError('Invalid bounded compute program set')
    baseline=lineage.analyze(data)
    extended=copy.deepcopy(data);images={};buffers={};influence=[];seen=set()
    for index,event in enumerate(extended['events']):
        parents={images[tuple(raw)] for raw in event['reads'] if raw[1]}
        parents.update(buffers[raw[3]] for raw in event.get('buffer_reads',[]) if raw[3])
        seed=event['kind']==1 and event.get('program') in programs
        if seed:seen.add(event['program']);event['seed']=True
        dependent=seed or any(influence[parent] for parent in parents)
        influence.append(dependent)
        event['dependent']=event['dependent'] or dependent
        for raw in event['writes']:images[tuple(raw)]=index
        for raw in event.get('buffer_writes',[]):buffers[raw[3]]=index
    candidate=lineage.analyze(extended)
    return dict(schema=1,observed_compute_camera_programs=len(programs),programs_seen_in_frame=len(seen),
        programs_not_seen_in_frame=len(programs-seen),
        added_seed_events_in_present_slice=candidate['camera_seed_events_in_slice']-baseline['camera_seed_events_in_slice'],
        added_dependent_events_in_present_slice=candidate['camera_dependent_events_in_slice']-baseline['camera_dependent_events_in_slice'],
        baseline=baseline,conservative_candidate=candidate,association_manifest_verified=False,
        per_dispatch_camera_semantics_confirmed=False,coverage_complete=False,replay_ready=False,
        scope='Conservative planning only: first binding observations are not per-dispatch semantic proof')

def bounded_json(path,limit):
    if path.is_symlink() or path.stat().st_size>limit:raise ValueError('Invalid bounded diagnostic file')
    return json.loads(path.read_text(encoding='utf-8-sig'))

def association(root):
    root=Path(root).resolve();manifest=bounded_json(root/'capture-association.json',16384)
    if manifest.get('schema')!=1 or manifest.get('producer')!='vr/run-game.ps1' or manifest.get('same_process_capture') is not True:
        raise ValueError('Missing same-process diagnostic association')
    for filename,key,limit in [('image-lineage.json','lineage_sha256',64*1024*1024),('program-bindings.jsonl','bindings_sha256',4*1024*1024)]:
        path=root/filename
        if path.is_symlink() or path.stat().st_size>limit:raise ValueError('Invalid associated diagnostic path/size')
        expected=manifest.get(key)
        if not isinstance(expected,str) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected.lower():
            raise ValueError('Associated diagnostic hash mismatch')
    return root

def analyze_directory(directory):
    root=association(directory);audit=bindings.analyze_directory(root)
    if audit['missing_observed_modules'] or audit['query_errors']:raise ValueError('Shader audit has missing/invalid modules')
    records=[json.loads(line) for line in (root/'program-bindings.jsonl').read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    programs={record['program'] for record in records if any(b['nvn_stage']==5 and
        (b['primary_camera_binding'] or b.get('candidate_view_offsets') or b.get('candidate_vp_offsets')) for b in record['bindings'])}
    result=analyze(bounded_json(root/'image-lineage.json',64*1024*1024),programs)
    result['association_manifest_verified']=True;return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path);args=parser.parse_args()
    try:result=analyze_directory(args.directory)
    except (OSError,ValueError,TypeError,KeyError,IndexError) as error:parser.exit(2,str(error)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
