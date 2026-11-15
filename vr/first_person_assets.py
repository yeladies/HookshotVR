"""Read the user's local Link bind poses; never ship the generated profile.

FRES skeleton layout reference (format facts; no library code copied):
https://github.com/KillzXGaming/BfresLibrary/tree/master/BfresLibrary/Shared/Models/Skeleton
Only little-endian FRES 10.4 with one validated skeleton is accepted.
"""
import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path


def multiply(a, b):
    return [sum(a[4*r+k]*b[4*k+c] for k in range(4)) for r in range(4) for c in range(4)]


def local_matrix(scale, rotation, position, euler):
    if not all(math.isfinite(v) for v in (*scale, *rotation, *position)):
        raise ValueError('non-finite bone transform')
    if euler:
        x, y, z = rotation[:3]
        cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
        r = [cz*cy, cz*sy*sx-sz*cx, cz*sy*cx+sz*sx,
             sz*cy, sz*sy*sx+cz*cx, sz*sy*cx-cz*sx,
             -sy, cy*sx, cy*cx]
    else:
        norm = math.sqrt(sum(v*v for v in rotation))
        if norm < 1e-12:
            raise ValueError('zero bone quaternion')
        x, y, z, w = (v/norm for v in rotation)
        r = [1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
             2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
             2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]
    return [r[0]*scale[0],r[1]*scale[1],r[2]*scale[2],position[0],
            r[3]*scale[0],r[4]*scale[1],r[5]*scale[2],position[1],
            r[6]*scale[0],r[7]*scale[1],r[8]*scale[2],position[2],0,0,0,1]


def bind_eye_profile(raw):
    if len(raw) < 64 or raw[:4] != b'FRES' or raw[8:12] != bytes((0,4,10,0)) or raw[12:14] != b'\xff\xfe':
        raise ValueError('unsupported FRES version or byte order')
    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(raw):
            raise ValueError('truncated skeleton')
        return struct.unpack_from(fmt, raw, offset)
    def skeleton(offset):
        array, = unpack('<Q', offset+16)
        count, = unpack('<H', offset+56)
        if not 1 <= count <= 4096 or array < 64 or array+88*count > len(raw):
            raise ValueError('invalid bone array')
        bones = []
        names = {}
        for k in range(count):
            at = array+88*k
            name, = unpack('<Q',at)
            length, = unpack('<H',name)
            if not 1 <= length <= 256 or name+2+length >= len(raw) or raw[name+2+length] != 0:
                raise ValueError('invalid bone name')
            name = raw[name+2:name+2+length].decode('utf-8',errors='strict')
            index, parent = unpack('<Hh',at+32)
            if index != k or not -1 <= parent < count or name in names:
                raise ValueError('invalid bone identity')
            flags, = unpack('<I',at+44)
            local = local_matrix(unpack('<3f',at+48),unpack('<4f',at+60),unpack('<3f',at+76),bool(flags & 0x1000))
            bones.append((parent,local))
            names[name] = k
        if not all(name in names for name in ('Head','Eyeball_L','Eyeball_R')):
            raise ValueError('missing head or eye bones')
        world, pending = {}, set()
        def resolve(k):
            if k in world:
                return world[k]
            if k in pending:
                raise ValueError('cyclic skeleton')
            pending.add(k)
            parent, local = bones[k]
            world[k] = multiply(resolve(parent),local) if parent >= 0 else local
            pending.remove(k)
            return world[k]
        # Validate every bone, including unused branches.
        for k in range(count):
            resolve(k)
        for eye in ('Eyeball_L','Eyeball_R'):
            parent = names[eye]
            while parent >= 0 and parent != names['Head']:
                parent = bones[parent][0]
            if parent < 0:
                raise ValueError('eyes do not descend from head')
        left, right = world[names['Eyeball_L']], world[names['Eyeball_R']]
        midpoint = [(left[4*k+3]+right[4*k+3])*.5 for k in range(3)]
        if not all(math.isfinite(v) for v in midpoint) or midpoint[1] <= 0:
            raise ValueError('invalid bind eye position')
        return {'eye_height':midpoint[1], 'forward_offset':midpoint[2],
                'lateral_offset':midpoint[0], 'bone_count':count,
                'model_sha256':hashlib.sha256(raw).hexdigest()}
    matches = []
    offset = raw.find(b'FSKL')
    while offset >= 0:
        try:
            matches.append(skeleton(offset))
        except (ValueError,UnicodeError,struct.error,RecursionError):
            pass
        offset = raw.find(b'FSKL',offset+4)
    if len(matches) != 1:
        raise ValueError('expected one unambiguous validated player skeleton')
    return matches[0]


def generate(romfs, units_per_metre):
    if not math.isfinite(units_per_metre) or not .001 <= units_per_metre <= 1000:
        raise ValueError('invalid explicit world scale')
    import zstandard as zs
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'nx2'))
    import shader_census as census
    dictionaries = census.dictionaries(romfs,zs)
    models = {}
    for age in ('Child','Adult'):
        source = Path(romfs)/'Model'/f'Player_{age}.Player_{age}.bfres.zs'
        raw = census.unpack(source.read_bytes(),dictionaries,zs)
        models[age.lower()] = bind_eye_profile(raw)
    return {'schema':1,'private_game_derived':True,'units_per_metre':units_per_metre,
            'physical_scale_calibrated':False,'anchor':'player_root_plus_bind_eye_no_animation_bob','models':models}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--romfs',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    parser.add_argument('--units-per-metre',required=True,type=float)
    args = parser.parse_args()
    result = generate(args.romfs,args.units_per_metre)
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print('Generated private eye profile; do not publish it. Physical scale still needs headset calibration.')


if __name__ == '__main__':
    main()
