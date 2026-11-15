"""Create a private first-person model overlay from the user's own game.

Only triangle index bytes are changed. Sizes, counts, bindings and vertices stay
unchanged. This diagnostic mask also removes head shadows/cutscene geometry;
a future render-only mask should replace it before normal release use.
Format facts: KillzXGaming/BfresLibrary Switch skeleton/shape/vertex readers.
No game data or generated models may be published.
"""
import argparse
import hashlib
import importlib.util
import json
import math
import struct
import sys
from pathlib import Path


def hide_head(raw, threshold=.25):
    if not math.isfinite(threshold) or not 0 < threshold <= 1:
        raise ValueError('invalid head weight threshold')
    # The bind-pose reader validates every bone and rejects ambiguous skeletons.
    spec = importlib.util.spec_from_file_location('first_person_assets',Path(__file__).with_name('first_person_assets.py'))
    assets = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(assets)
    assets.bind_eye_profile(raw)
    def read(fmt, at):
        if at < 0 or at+struct.calcsize('<'+fmt) > len(raw):
            raise ValueError('resource outside file')
        return struct.unpack_from('<'+fmt,raw,at)
    def text(at):
        n, = read('H',at)
        if not 1 <= n <= 256 or at+2+n >= len(raw) or raw[at+2+n] != 0:
            raise ValueError('invalid resource string')
        return raw[at+2:at+2+n].decode('utf-8',errors='strict')
    # External-string references (bit 1) may coexist with the inline bone and
    # shape names validated below. Separate GPU data and string-holder files
    # require a different reader and remain rejected.
    if raw[238] & ~2:
        raise ValueError('external GPU/string-holder sections are unsupported')
    skeleton = raw.index(b'FSKL')
    bones, mapping_at = read('QQ',skeleton+16)
    count, smooth, rigid = read('HHH',skeleton+56)
    mapping = read('H'*(smooth+rigid),mapping_at)
    if not mapping or any(k >= count for k in mapping):
        raise ValueError('invalid skin matrix mapping')
    parents = [read('h',bones+88*k+34)[0] for k in range(count)]
    heads = [k for k in range(count) if text(read('Q',bones+88*k)[0]) == 'Head']
    if len(heads) != 1:
        raise ValueError('ambiguous head')
    head_bones = set()
    for k in range(count):
        parent = k
        while parent >= 0:
            if parent == heads[0]:
                head_bones.add(k)
                break
            parent = parents[parent]
    info, = read('Q',176)
    buffer_base, = read('Q',info+8)
    if not 0 < buffer_base < len(raw):
        raise ValueError('invalid GPU buffer section')
    index_formats = {0x302:'B',0x309:'BB',0x30b:'BBBB',0x30a:'H',0x312:'HH',0x315:'HHHH'}
    weight_formats = {0x102:'B',0x109:'BB',0x10b:'BBBB'}
    result = bytearray(raw)
    changed = set()
    reports = []
    shape = raw.find(b'FSHP')
    while shape >= 0:
        name_at, vertex, meshes = read('QQQ',shape+8)
        name = text(name_at)
        skin = raw[shape+90]
        mesh_count = raw[shape+91]
        if not 1 <= skin <= 8 or not 1 <= mesh_count <= 16 or raw[vertex:vertex+4] != b'FVTX':
            raise ValueError('unsupported player shape')
        attributes, = read('Q',vertex+8)
        sizes, strides = read('QQ',vertex+48)
        relative, = read('I',vertex+72)
        num_attributes, num_buffers = raw[vertex+76:vertex+78]
        vertices, = read('I',vertex+80)
        alignment, = read('H',vertex+86)
        if not 1 <= num_attributes <= 64 or not 1 <= num_buffers <= 8 or not 1 <= vertices <= 1000000 or not alignment or alignment & (alignment-1):
            raise ValueError('invalid vertex layout')
        streams = []
        start = buffer_base+relative
        for k in range(num_buffers):
            start = (start+alignment-1)&~(alignment-1)
            size, = read('I',sizes+16*k)
            stride, = read('I',strides+16*k)
            if stride <= 0 or start < buffer_base or start+size > len(raw) or size < vertices*stride:
                raise ValueError('invalid vertex stream')
            streams.append((start,size,stride))
            start += size
        attrs = {}
        for k in range(num_attributes):
            at = attributes+16*k
            attr = text(read('Q',at)[0])
            if attr in attrs:
                raise ValueError('duplicate vertex attribute')
            fmt, = struct.unpack_from('>H',raw,at+8)
            offset, stream = read('HH',at+12)
            if stream >= len(streams):
                raise ValueError('invalid attribute stream')
            attrs[attr] = (fmt,offset,stream)
        def values(prefix, vertex_index, formats):
            out = []
            for k in range(2):
                item = attrs.get(prefix+str(k))
                if item is None:
                    continue
                fmt, offset, stream = item
                layout = formats.get(fmt)
                if layout is None:
                    raise ValueError('unsupported skin attribute format')
                start,size,stride = streams[stream]
                width = struct.calcsize('<'+layout)
                if offset+width > stride:
                    raise ValueError('skin attribute crosses vertex stride')
                out.extend(read(layout,start+vertex_index*stride+offset))
            return out[:skin]
        masked = []
        for k in range(vertices):
            ids = values('_i',k,index_formats)
            weights = values('_w',k,weight_formats) if skin > 1 else [255]
            if len(ids) != skin or len(weights) != skin or not 253 <= sum(weights) <= 257:
                raise ValueError('invalid skin weights')
            if any(index >= len(mapping) for index,weight in zip(ids,weights) if weight):
                raise ValueError('skin index outside skeleton mapping')
            masked.append(sum(weight for index,weight in zip(ids,weights) if weight and mapping[index] in head_bones) >= threshold*sum(weights))
        removed = total = 0
        for k in range(mesh_count):
            at = meshes+56*k
            relative, primitive, index_format, indices, first = read('IIIII',at+32)
            size, = read('I',read('Q',at+24)[0])
            width = {1:2,2:4}.get(index_format)
            start = buffer_base+relative
            if primitive != 3 or width is None or indices % 3 or size != indices*width or start < buffer_base or start+size > len(raw):
                raise ValueError('unsupported triangle index buffer')
            fmt = 'H' if width == 2 else 'I'
            for triangle in range(indices//3):
                pos = start+3*width*triangle
                original = read(fmt*3,pos)
                if any(index+first >= vertices for index in original):
                    raise ValueError('triangle index outside vertex buffer')
                total += 1
                if any(masked[index+first] for index in original):
                    # Repeated indices produce no surface in every LOD. Counts,
                    # submesh ranges and the resource's uncompressed size stay valid.
                    data = struct.pack('<'+fmt*3,original[0],original[0],original[0])
                    for offset,value in enumerate(data):
                        target = pos+offset
                        if result[target] != value:
                            result[target] = value
                            changed.add(target)
                    removed += 1
        reports.append({'shape':name,'triangles':total,'removed':removed,'head_vertices':sum(masked)})
        shape = raw.find(b'FSHP',shape+4)
    if not reports or not changed:
        raise ValueError('no head geometry was masked')
    # No model headers, skeleton or vertex data may be changed.
    return bytes(result), {'private_game_derived':True,'source_sha256':hashlib.sha256(raw).hexdigest(),
        'output_sha256':hashlib.sha256(result).hexdigest(),'changed_index_bytes':len(changed),
        'same_uncompressed_size':len(result)==len(raw),'threshold':threshold,'shapes':reports}


def generate_overlay(romfs, output):
    import zstandard as zs
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'nx2'))
    import shader_census as census
    if output.exists():
        raise ValueError('head mask output must be a fresh directory')
    dictionaries = census.dictionaries(romfs,zs)
    pending = []
    report = {'schema':1,'private_game_derived':True,'all_passes_including_shadows':True,'models':{}}
    for age in ('Child','Adult'):
        relative = Path('Model')/f'Player_{age}.Player_{age}.bfres.zs'
        packed = (romfs/relative).read_bytes()
        raw = census.unpack(packed,dictionaries,zs)
        masked,details = hide_head(raw)
        dictionary_id = zs.get_frame_parameters(packed).dict_id
        dictionary = None
        if dictionary_id:
            for data in dictionaries:
                candidate = zs.ZstdCompressionDict(data,dict_type=zs.DICT_TYPE_FULLDICT)
                if candidate.dict_id() == dictionary_id:
                    dictionary = candidate
                    break
            if dictionary is None:
                raise ValueError('source compression dictionary not found')
        compressor = zs.ZstdCompressor(level=8,dict_data=dictionary)
        encoded = compressor.compress(masked)
        if census.unpack(encoded,dictionaries,zs) != masked:
            raise ValueError('overlay round trip failed')
        pending.append((relative,encoded))
        report['models'][age.lower()] = details
    # All source layouts validated before creating any output file.
    for relative,data in pending:
        target = output/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(data)
    (output/'head-mask-private-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--romfs',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args()
    generate_overlay(args.romfs,args.out)
    print('Created private head-only geometry overlay. Do not publish generated models or reports.')


if __name__=='__main__':main()
