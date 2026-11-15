"""Join private temporal matrix candidates to conservative static shader reads.

A read footprint does not prove the meaning of the loaded values. No modules,
guest buffers or renderer resources are modified by this tool.
"""
import argparse
import importlib.util
import json
from pathlib import Path


def load(name, filename):
    spec=importlib.util.spec_from_file_location(name,Path(__file__).with_name(filename))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


history=load('temporal_history','audit_camera_history.py')
bindings=load('camera_bindings','audit_shader_bindings.py')
reader=bindings.reader
WIDTHS={'view_3x4':48,'view_projection_4x4':64,'projection_4x4':64,
        'inverse_view_3x4':48,'inverse_projection_4x4':64,'inverse_view_projection_4x4':64}


def analyze(history_directory,shader_directory):
    temporal=history.analyze(history_directory)
    root=Path(shader_directory).resolve()
    summary=bindings.analyze_directory(root) # Validate bounded paths, identities and metadata first.
    records=[json.loads(line) for line in (root/'program-bindings.jsonl').read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    index=Path(history_directory)/'index.jsonl'
    captured_program=json.loads(next(line for line in index.read_text(encoding='utf-8-sig').splitlines() if line.strip()))['program']
    queries={};consumers=[];missing=0
    for record in records:
        for binding in record['bindings']:
            if not binding['primary_camera_binding']:continue
            if not binding.get('module'):
                missing+=1;continue
            key=tuple(binding[k] for k in ('module','binding','array_element','range_bytes'))
            if key not in queries:
                try:queries[key]=reader.analyze(reader.read_module(root/(binding['module']+'.spv')),binding['binding'],0,binding['array_element'],binding['range_bytes'])
                except (OSError,ValueError,IndexError,TypeError) as error:queries[key]={'error':str(error),'static_read_word_offsets':[],'static_subset_resolved':False}
            result=queries[key];reads=set(result['static_read_word_offsets'])
            regions=[]
            for kind,candidates in temporal['numeric_matrix_candidates'].items():
                for candidate in candidates:
                    start=candidate['offset_bytes'];end=start+WIDTHS[kind]
                    overlap=sorted(word for word in reads if start<=word<end)
                    if overlap:regions.append(dict(candidate,matrix_kind=kind,static_read_word_count=len(overlap),candidate_word_count=WIDTHS[kind]//4))
            consumers.append({'program':record['program'],'same_program_as_history':record['program']==captured_program,
                'nvn_stage':binding['nvn_stage'],'vk_stage':binding['vk_stage'],'binding':binding['binding'],
                'array_element':binding['array_element'],'range_bytes':binding['range_bytes'],'module':binding['module'],
                'static_subset_resolved':result['static_subset_resolved'],'numeric_regions_read':regions})
    return {'schema':1,'temporal_samples':temporal['samples'],'observed_programs':summary['observed_programs'],
            'captured_program_observed':any(r['program']==captured_program for r in records),
            'primary_binding_consumers':len(consumers),'missing_primary_modules':summary['missing_primary_modules'],
            'unresolved_primary_queries':sum(not value['static_subset_resolved'] for value in queries.values()),
            'query_errors':sorted({value['error'] for value in queries.values() if 'error' in value}),
            'consumers':consumers,'producer_history_semantics_verified':False,'complete_shader_semantic_proof':False,
            'safe_to_patch':False,'scope':'Numerical candidate/static-load intersection; representative programs only'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('history_directory',type=Path);parser.add_argument('shader_directory',type=Path)
    parser.add_argument('--out',required=True,type=Path);args=parser.parse_args()
    try:result=analyze(args.history_directory,args.shader_directory)
    except (OSError,ValueError,IndexError,TypeError,KeyError) as error:parser.exit(2,str(error)+'\n')
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Private consumer footprint saved; complete semantics and patch safety remain unverified.')


if __name__=='__main__':main()
