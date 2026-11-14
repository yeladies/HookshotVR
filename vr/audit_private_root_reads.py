# SPDX-License-Identifier: GPL-3.0-or-later
"""Inventory global-read shader effects without enabling private-root replay."""
import argparse
import importlib.util
import json
from pathlib import Path
import struct

_spec=importlib.util.spec_from_file_location('vr_root_program_inventory',Path(__file__).with_name('audit_graphics_programs.py'))
programs=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(programs)

def analyze_directory(directory,grammar,*,require_association=True):
    root=Path(directory).resolve()
    original=programs.analyze_directory(root,grammar,require_association)
    records=[json.loads(line) for line in (root/'program-bindings.jsonl').read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    effects={};rows=[]
    for record,prior in zip(records,original['programs'],strict=True):
        stage_rows=[];complete=True;readonly=True;uses_global=False
        for stage,previous in zip(record.get('stages',[]),prior['stages'],strict=True):
            stores=stage.get('stores_global_memory')
            if 'stores_global_memory' in stage and type(stores) is not bool:raise ValueError('Global-write metadata must be a boolean')
            complete &= type(stores) is bool
            native_readonly=stores is False and not any(stage[flag] for flag in programs.WRITES if flag!='uses_global_memory')
            uses_global |= stage['uses_global_memory']
            digest=stage.get('module');report=None
            if previous['module_available']:
                if digest not in effects:
                    path=root/(digest+'.spv');data=path.read_bytes()
                    if len(data)>8*1024*1024 or len(data)%4 or programs.fingerprint(data)!=digest:raise ValueError('Stage changed during effect inventory')
                    effects[digest]=programs.effects.analyze(struct.unpack('<'+'I'*(len(data)//4),data),grammar,private_root_read_inventory=True)
                report=effects[digest]
            expected={1:0,16:4}.get(stage['vk_stage'])
            subset=bool(report and report['private_root_read_inventory_subset'] and report['entry_models']==[expected])
            readonly &= native_readonly and subset
            stage_rows.append({'vk_stage':stage['vk_stage'],'nvn_stage':stage['nvn_stage'],'module_fingerprint':digest,
                'uses_global_memory':stage['uses_global_memory'],'stores_global_memory':stores,'native_effects_read_only':native_readonly,
                'physical_read_effect_subset':subset,'effects':report})
        candidate=bool(prior['stage_inventory_declared_complete'] and prior['all_declared_stage_files_available'] and
            {s['vk_stage'] for s in stage_rows}=={1,16} and complete and readonly and uses_global and prior['primary_binding_observed'])
        rows.append({'program':record['program'],'uses_global_memory':uses_global,'native_global_write_metadata_complete':complete,
            'global_read_effect_candidate':candidate,'vertex_camera_transform_reads_observed':prior['vertex_camera_transform_reads_observed'],
            'eye_patch_preflight':prior['eye_patch_preflight'],'stages':stage_rows,'private_root_routing_verified':False,
            'private_root_draw_ready':False,'safe_to_replay':False})
    return {'schema':1,'observed_programs':len(rows),'unique_modules':len(effects),
        'same_process_manifest_verified':original['same_process_manifest_verified'],
        'global_read_effect_candidates':sum(r['global_read_effect_candidate'] for r in rows),
        'global_programs_missing_write_metadata':sum(r['uses_global_memory'] and not r['native_global_write_metadata_complete'] for r in rows),
        'safe_to_replay':False,'private_root_draw_ready':False,'programs':rows,
        'limitations':['Effect inventory only; no draw selection file is generated.',
            'Does not prove physical pointer provenance, private camera coverage, descriptor aliases, pipeline state or lifetime.',
            'Native metadata and SPIR-V effects must agree; older captures without explicit global-write flags remain unverified.',
            'Complete scene rendering, physical IPD, UI composition and XR delivery are separate gates.']}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path)
    parser.add_argument('--grammar',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        if args.grammar.stat().st_size>8*1024*1024:raise ValueError('Grammar exceeds input budget')
        report=analyze_directory(args.directory,json.loads(args.grammar.read_text(encoding='utf-8')))
        args.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    except (OSError,ValueError,KeyError,TypeError) as error:parser.error(str(error))
    print(json.dumps({k:report[k] for k in ('observed_programs','global_read_effect_candidates','global_programs_missing_write_metadata','private_root_draw_ready')}))

if __name__=='__main__':main()
