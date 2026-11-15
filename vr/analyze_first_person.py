"""Check private first-person game logs; this never certifies headset visuals."""
import argparse
import json
import math
import re
from pathlib import Path

NUMBER=r'[-+0-9.eE]+'
VECTOR=rf'({NUMBER}),({NUMBER}),({NUMBER})'
SAMPLE=re.compile(rf'vr-first-person: (synthetic|tracked) actor=0x[0-9a-fA-F]+ age=(child|adult) seq=(\d+) origin=(\d+) root={VECTOR} eye={VECTOR} height=({NUMBER}) forward=({NUMBER}) units=({NUMBER}) flags=(\d+)(?: direction={VECTOR})?')


def analyze(report):
    text=Path(report).read_text(encoding='utf-8-sig')
    data=json.loads(text[text.index('{'):])
    samples=[]
    for match in SAMPLE.finditer((Path(data['runtime_root'])/'stderr.txt').read_text(encoding='utf-8',errors='replace')):
        values=match.groups();numbers=[float(v) for v in values[4:13]]
        if not all(math.isfinite(v) for v in numbers):
            raise ValueError('non-finite camera log')
        samples.append({'kind':values[0],'age':values[1],'sequence':int(values[2]),'origin':int(values[3]),
                        'root':numbers[:3],'eye':numbers[3:6],'height':numbers[6],'forward':numbers[7],'units':numbers[8],'flags':int(values[13]),'direction':[float(v) for v in values[14:17]] if values[14] is not None else None})
    if len(samples)<4 or data['reason']!='time_limit':
        raise ValueError('incomplete first-person run')
    if not data.get('first_person_readback_requested') or data.get('native_stereo_verified') or data.get('xr_session_requested') or data.get('xr_negotiation_requested'):
        raise ValueError('expected a non-XR first-person diagnostic')
    if data.get('first_person_head_mask_requested') and not data.get('first_person_head_mask_loaded'):
        raise ValueError('head mask was not actually loaded')
    last_sequence=0
    for sample in samples:
        if sample['kind']!='synthetic' or not sample['flags']&2 or sample['sequence']<last_sequence or sample['units']<=0:
            raise ValueError('invalid synthetic packet provenance')
        last_sequence=sample['sequence']
        if data['first_person_sweep']!='Lean':
            delta=[e-r for e,r in zip(sample['eye'],sample['root'])]
            if abs(delta[1]-sample['height'])>.002 or abs(math.hypot(delta[0],delta[2])-abs(sample['forward']))>.002:
                raise ValueError('camera escaped the fixed root/eye anchor')
    directions=[sample['direction'] for sample in samples if sample['direction'] is not None]
    yaw_range=pitch_range=None
    if directions:
        if any(not all(math.isfinite(v) for v in direction) or abs(math.sqrt(sum(v*v for v in direction))-1)>.001 for direction in directions):
            raise ValueError('invalid normalized camera direction')
        headings=[]
        for x,y,z in directions:
            angle=math.atan2(x,z)
            headings.append(angle if not headings else headings[-1]+math.remainder(angle-headings[-1],2*math.pi))
        yaw_range=max(headings)-min(headings)
        pitch_range=max(v[1] for v in directions)-min(v[1] for v in directions)
    if data['first_person_sweep']=='WideYaw' and (yaw_range is None or yaw_range<1.5):
        raise ValueError('wide horizontal head sweep not observed')
    if data['first_person_sweep']=='WidePitch' and (pitch_range is None or pitch_range<1.4):
        raise ValueError('wide vertical head sweep not observed')
    if data.get('first_person_scene_audit_requested') and not (data.get('scene_camera_matched') and data.get('scene_submission_matched')):
        raise ValueError('new camera was not matched to native scene draws')
    root_ranges=[max(s['root'][k] for s in samples)-min(s['root'][k] for s in samples) for k in range(3)]
    relative_ranges=[max(s['eye'][k]-s['root'][k] for s in samples)-min(s['eye'][k]-s['root'][k] for s in samples) for k in range(3)]
    if data.get('first_person_move_requested') and (not data.get('first_person_move_applied') or max(root_ranges)<.05):
        raise ValueError('scripted stick motion did not move the actual player root')
    if data['first_person_sweep']=='Lean' and max(relative_ranges)<.05:
        raise ValueError('physical head translation was not observed')
    return {'source_fingerprint':data.get('source_fingerprint'),'mode':data['first_person_sweep'],
            'yaw_range_radians':yaw_range,'direction_y_range':pitch_range,'native_scene_camera_matched':bool(data.get('scene_camera_matched')),'samples':len(samples),'camera_log_contract_verified':True,'head_mask_loaded':data.get('first_person_head_mask_loaded',False),
            'root_motion_observed':max(root_ranges)>=.05,'physical_translation_observed':max(relative_ranges)>=.05,
            'root_ranges_world_units':root_ranges,'eye_relative_to_root_ranges':relative_ranges,
            'runtime_root':data['runtime_root'],'shots':data['first_person_shots'],
            'visual_acceptance':False,'native_stereo_verified':False,'headset_opened':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('report',type=Path);parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args();result=analyze(args.report)
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('First-person log contract passed. Visual and headset acceptance remain separate.')


if __name__=='__main__':main()
