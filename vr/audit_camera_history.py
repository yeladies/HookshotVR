"""Analyze private moving-camera copies; numerical matches are not patch permission."""
import argparse
import hashlib
import importlib.util
import json
import math
import re
import struct
from pathlib import Path

spec=importlib.util.spec_from_file_location('camera_uniform_math',Path(__file__).with_name('audit_camera_uniforms.py'))
helpers=importlib.util.module_from_spec(spec);spec.loader.exec_module(helpers)


def close(a,b):
    return len(a)==len(b) and all(math.isfinite(x) and math.isfinite(y) and abs(x-y)<=.0002+.000002*max(abs(x),abs(y)) for x,y in zip(a,b))


def projection_evidence(rows):
    evidence=[row['first_person_projection'] for row in rows if 'first_person_projection' in row]
    if evidence and len(evidence)!=len(rows):raise ValueError('mixed native projection audit coverage')
    available=[]
    for item in evidence:
        if not isinstance(item,dict) or any(type(item.get(k)) is not bool for k in ('available','center_matches','depth_exact','fov_matches','synthetic')):
            raise ValueError('invalid native projection evidence flags')
        if not item['available']:
            if any(item[k] for k in ('center_matches','depth_exact','fov_matches')):raise ValueError('unavailable projection cannot claim a match')
            continue
        if any(type(item.get(k)) is not int or item[k]<=0 for k in ('sequence','origin','predicted_time','observed_ns')):
            raise ValueError('invalid native projection identity')
        for k in ('effective_near','eye_separation_world'):
            if type(item.get(k)) not in (float,int) or not math.isfinite(item[k]) or item[k]<=0:raise ValueError('invalid native projection dimensions')
        far=item.get('effective_far')
        if far is not None and (type(far) not in (float,int) or not math.isfinite(far) or far<=item['effective_near']):raise ValueError('invalid native effective far')
        available.append(item)
    complete=len(available)==len(rows) and all(item['center_matches'] and item['depth_exact'] and item['fov_matches'] for item in available)
    return {'native_projection_samples':len(available),'native_projection_audit_coverage_complete':complete,
            'native_projection_center_matches':sum(item['center_matches'] for item in available),
            'native_projection_depth_exact_samples':sum(item['depth_exact'] for item in available),
            'native_projection_fov_matches_samples':sum(item['fov_matches'] for item in available),
            'native_projection_synthetic_samples':sum(item['synthetic'] for item in available),
            'native_projection_eye_separation_range':[min(item['eye_separation_world'] for item in available),max(item['eye_separation_world'] for item in available)] if available else []}


def analyze(directory):
    root=Path(directory).resolve();index=root/'index.jsonl'
    if index.is_symlink() or index.stat().st_size>256*1024:
        raise ValueError('invalid bounded capture index')
    rows=[json.loads(line) for line in index.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    if len(rows)!=32:
        raise ValueError('expected 32 complete history samples')
    values=[];references=[];identities=set();frames=[];digests=[];tagged=[]
    provenance_fields=('source_nvn_submit','observation_game_present','observation_present_ambiguous','primary_sequence','producer_present_verified')
    for row in rows:
        if row.get('schema')!=1 or row.get('scene_camera_match') is not True or row.get('submission_overlay_copy') is not True or row.get('gpu_completed_readback') is not False:
            raise ValueError('capture provenance differs from the read-only diagnostic')
        for field in ('frame','program','recording','camera_revision','source','width','height','bytes'):
            if type(row.get(field)) is not int or row[field]<=0:
                raise ValueError('invalid integer capture identity')
        if not 0xb00<=row['bytes']<=65536 or row['bytes']%4:
            raise ValueError('invalid bounded uniform range')
        if frames and row['frame']<=frames[-1]:
            raise ValueError('frames must increase strictly')
        frames.append(row['frame']);identities.add((row['program'],row['bytes'],row['width'],row['height']))
        filename=row.get('file','')
        if not isinstance(filename,str) or not re.fullmatch(r'frame-[0-9]{1,20}\.bin',filename):
            raise ValueError('invalid private payload filename')
        if filename!=f"frame-{row['frame']}.bin":raise ValueError('payload filename/frame mismatch')
        path=root/filename
        if path.is_symlink() or path.stat().st_size!=row['bytes']:
            raise ValueError('payload size or link rejected')
        has_provenance=any(field in row for field in provenance_fields);tagged.append(has_provenance)
        if has_provenance:
            if any(field not in row for field in provenance_fields) or any(type(row[field]) is not int or not 0<row[field]<(1<<64) for field in ('source_nvn_submit','primary_sequence')) or type(row['observation_game_present']) is not int or type(row['observation_present_ambiguous']) is not bool or row['producer_present_verified'] is not False:
                raise ValueError('invalid source-submit or observation metadata')
            present=row['observation_game_present']
            if (row['observation_present_ambiguous'] and present!=-1) or (not row['observation_present_ambiguous'] and not 0<=present<(1<<63)):
                raise ValueError('invalid or ambiguous observation present')
        data=path.read_bytes();digests.append(hashlib.sha256(data).hexdigest())
        block=list(struct.unpack('<'+'f'*(len(data)//4),data))
        if not all(math.isfinite(v) for v in block[:0x230//4]):
            raise ValueError('non-finite accepted camera prefix')
        view=block[:12]+[0,0,0,1];projection=block[0x70//4:0x70//4+16];vp=block[0x30//4:0x30//4+16]
        if not close(vp,helpers.multiply(projection,view)):
            raise ValueError('view/projection prefix is algebraically incoherent')
        inverse_view=helpers.inverse(view)
        references.append({'view_3x4':view[:12],'view_projection_4x4':vp,'projection_4x4':projection,
                           'inverse_view_3x4':inverse_view[:12],'inverse_projection_4x4':helpers.inverse(projection),
                           'inverse_view_projection_4x4':helpers.inverse(vp)})
        values.append(block)
    if any(tagged) and not all(tagged):
        raise ValueError('mixed capture provenance versions')
    if len(identities)!=1:
        raise ValueError('program, range and viewport must remain fixed')
    distinct=[]
    for ref in references:
        if not any(close(ref['view_3x4'],other) for other in distinct):distinct.append(ref['view_3x4'])
    if len(distinct)<8:
        raise ValueError('insufficient camera motion to distinguish time offsets')
    candidates={}
    for name in references[0]:
        records={};width=len(references[0][name])
        for lag in range(5):
            reference=references[0][name]
            starts=[at for at in range(0,len(values[lag])-width+1,4) if close(values[lag][at:at+width],reference)]
            for at in starts:
                if all(close(values[k][at:at+width],references[k-lag][name]) for k in range(lag+1,len(values))):
                    records.setdefault(at*4,[]).append(lag)
        candidates[name]=[{'offset_bytes':offset,'matching_sample_lags':lags,'unique_sample_lag':lags[0] if len(lags)==1 else None}
                          for offset,lags in sorted(records.items())]
    return {**projection_evidence(rows),'schema':1,'samples':len(rows),'distinct_views':len(distinct),'payload_bytes':rows[0]['bytes'],
            'frame_interval_values':sorted(set(b-a for a,b in zip(frames,frames[1:]))),
            'observation_tagged_samples':sum('observation_game_present' in row for row in rows),'ambiguous_observation_samples':sum(row.get('observation_present_ambiguous') is True for row in rows),'producer_present_verified':False,'source_nvn_submit_count':len({row['source_nvn_submit'] for row in rows if 'source_nvn_submit' in row}),'recording_differs_from_source_samples':sum(row['recording']!=row['source_nvn_submit'] for row in rows if 'source_nvn_submit' in row),'numeric_matrix_candidates':candidates,'submission_overlay_copy':True,'gpu_completed_readback':False,
            'shader_consumption_verified':False,'producer_history_semantics_verified':False,'safe_to_patch':False,
            'payload_sha256':digests,'remaining':['verify producer meanings and shader consumers','independent per-eye resources and temporal histories']}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('directory',type=Path);parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args()
    try:result=analyze(args.directory)
    except (OSError,ValueError,IndexError,TypeError,KeyError) as error:parser.exit(2,str(error)+'\n')
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Temporal numeric evidence saved; shader/producer semantics and patch safety remain unverified.')


if __name__=='__main__':main()
