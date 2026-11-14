# SPDX-License-Identifier: GPL-3.0-or-later
"""Find algebraic candidates in bounded completed-GPU camera captures; never patch."""
import argparse
import importlib.util
import json
import math
from pathlib import Path
import struct

_spec=importlib.util.spec_from_file_location('vr_payload_matrix_tools',Path(__file__).with_name('audit_camera_uniforms.py'))
matrices=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(matrices)
PROOFS=('gpu_completion_verified','source_unchanged','camera_aliases_verified','matrix_payloads_verified','indirect_guard_verified')

def decode(sample):
    if type(sample.get('schema')) is not int or sample.get('schema')!=1 or sample.get('kind')!='completed_gpu_camera_payload' or any(sample.get(k) is not True for k in PROOFS):raise ValueError('Missing completed GPU source evidence')
    for field in ('serial','program','recording','guest','pool_generation','capture_generation'):
        if type(sample.get(field)) is not int or not 0<sample[field]<(1<<64):raise ValueError('Invalid camera source scope')
    for field in ('width','height'):
        if type(sample.get(field)) is not int or not 0<sample[field]<=4096:raise ValueError('Invalid camera source size')
    words=sample.get('words');size=sample.get('bytes')
    if type(size) is not int or not 560<=size<=65536 or size%4 or not isinstance(words,list) or len(words)!=size//4 or any(type(w) is not int or not 0<=w<=0xffffffff for w in words):raise ValueError('Invalid bounded camera words')
    guest=sample['guest'];end=guest+size
    if guest%4 or not (0x1000000000<=guest<end<=0x1200000000 or 0x7f00000000<=guest<end<=0x8000000000):raise ValueError('Camera range crosses guest windows')
    values=list(struct.unpack('<'+'f'*len(words),struct.pack('<'+'I'*len(words),*words)))
    if any(not math.isfinite(x) for x in values[:56]):raise ValueError('Nonfinite camera matrix prefix')
    view=values[:12]+[0.,0.,0.,1.];projection=values[28:44];vp=values[12:28];inverse_view=matrices.inverse(view)
    if not matrices.close(vp,matrices.multiply(projection,view)) or not matrices.close(values[44:56],inverse_view[:12]):raise ValueError('Incoherent source matrices')
    references={'view_3x4':view[:12],'view_projection_4x4':vp,'projection_4x4':projection,'inverse_view_3x4':inverse_view[:12],
        'inverse_view_projection_4x4':matrices.inverse(vp),'inverse_projection_4x4':matrices.inverse(projection),
        'world_camera_position_xyz':[inverse_view[3],inverse_view[7],inverse_view[11]]}
    return values,view,references

def audit(samples):
    if not 1<=len(samples)<=8:raise ValueError('Expected one to eight completed camera samples')
    common=None;views=[];sizes=set();nonfinite=0;distinct={}
    for sample in samples:
        values,view,references=decode(sample);sizes.add(len(values)*4)
        if len(sizes)!=1:raise ValueError('Mixed canonical camera extents')
        nonfinite+=sum(not math.isfinite(x) for x in values[56:])
        if not any(matrices.close(view,old) for old in views):views.append(view)
        found={}
        for name,reference in references.items():
            known=distinct.setdefault(name,[])
            if not any(matrices.close(reference,old,.0002,.000002) for old in known):known.append(reference)
            step=1 if name.endswith('_xyz') else 4
            found[name]={offset*4 for offset in range(0,len(values)-len(reference)+1,step)
                if matrices.close(values[offset:offset+len(reference)],reference,.0002,.000002)}
        common=found if common is None else {name:common[name]&found[name] for name in common}
    return {'schema':1,'samples':len(samples),'payload_bytes':sizes.pop(),'distinct_views':len(views),
        'multiple_view_samples':len(views)>=3,'nonfinite_preserved_words':nonfinite,
        'reference_distinct_values':{name:len(values) for name,values in distinct.items()},
        'common_algebraic_candidates':{name:sorted(offsets)[:128] for name,offsets in common.items()},
        'candidate_counts':{name:len(offsets) for name,offsets in common.items()},
        'candidate_output_truncated':any(len(offsets)>128 for offsets in common.values()),
        'source_provenance_independently_authenticated':False,'shader_consumption_verified':False,'field_semantics_confirmed':False,'safe_to_patch':False,
        'limitations':['Capture flags describe native observations, not independent authentication of an arbitrary JSON file.',
            'Matching values can coincide; multiple views and reference-value variation are reported separately.',
            'Unknown words retain their source bits; nonfinite preserved words do not imply invalid matrices.',
            'No field is changed, no eye correctness or full scene coverage is inferred.']}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path,nargs='+');parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:
        if not 1<=len(args.capture)<=8:raise ValueError('Expected one to eight files')
        for path in args.capture:
            if path.stat().st_size>256*1024:raise ValueError('Camera capture exceeds byte budget')
        report=audit([json.loads(path.read_text(encoding='utf-8-sig')) for path in args.capture]);text=json.dumps(report,indent=2)+'\n'
        if args.output:args.output.write_text(text,encoding='utf-8')
        else:print(text,end='')
    except (OSError,ValueError,TypeError,KeyError) as error:parser.error(str(error))

if __name__=='__main__':main()
