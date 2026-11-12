# SPDX-License-Identifier: GPL-3.0-or-later
"""Conservative static reads of one SPIR-V uniform binding; never modifies modules."""
import argparse
import json
import struct
from pathlib import Path

# Verified against the installed Khronos unified1 grammar. Normative reference:
# https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html
TYPE_INT, TYPE_FLOAT, TYPE_VECTOR, TYPE_MATRIX = 21, 22, 23, 24
TYPE_ARRAY, TYPE_RUNTIME_ARRAY, TYPE_STRUCT, TYPE_POINTER = 28, 29, 30, 32
CONSTANT, CONSTANT_NULL, VARIABLE, LOAD = 43, 46, 59, 61
ACCESS_CHAIN, INBOUNDS_ACCESS_CHAIN, PTR_ACCESS_CHAIN, INBOUNDS_PTR_ACCESS_CHAIN = 65, 66, 67, 70
DECORATE, MEMBER_DECORATE, COPY_OBJECT = 71, 72, 83
UNIFORM, BLOCK, ARRAY_STRIDE, BINDING, DESCRIPTOR_SET, OFFSET = 2, 2, 6, 33, 34, 35


def instructions(words):
    if len(words) < 5 or words[0] != 0x07230203 or not 0 < words[3] <= (1 << 24):
        raise ValueError('Invalid bounded SPIR-V header')
    index = 5
    while index < len(words):
        size, opcode = words[index] >> 16, words[index] & 0xffff
        if not size or index + size > len(words):
            raise ValueError('Truncated or zero-length SPIR-V instruction')
        yield opcode, words[index+1:index+size]
        index += size


def analyze(words, binding, descriptor_set=0, array_element=0, byte_limit=65536):
    if min(binding, descriptor_set, array_element) < 0 or not 0 < byte_limit <= 65536:
        raise ValueError('Invalid binding or uniform range')
    ops = list(instructions(words))
    types, constants, decorations, member_offsets = {}, {}, {}, {}
    for opcode, args in ops:
        if opcode in (TYPE_INT, TYPE_FLOAT, TYPE_VECTOR, TYPE_MATRIX, TYPE_ARRAY, TYPE_RUNTIME_ARRAY, TYPE_STRUCT, TYPE_POINTER):
            if not args: raise ValueError('Missing type ID')
            types[args[0]] = (opcode, args[1:])
        elif opcode == CONSTANT and len(args) >= 3:
            kind, data = types.get(args[0], (None, []))
            if kind == TYPE_INT and data[0] in (32, 64):
                value = args[2] | ((args[3] << 32) if data[0] == 64 and len(args) > 3 else 0)
                if data[1] and value & (1 << (data[0]-1)): value -= 1 << data[0]
                constants[args[1]] = value
        elif opcode == CONSTANT_NULL and len(args) == 2:
            if types.get(args[0], (None, []))[0] == TYPE_INT: constants[args[1]] = 0
        elif opcode == DECORATE and len(args) >= 2:
            decorations[(args[0], args[1])] = args[2] if len(args) > 2 else True
        elif opcode == MEMBER_DECORATE and len(args) >= 4 and args[2] == OFFSET:
            member_offsets[(args[0], args[1])] = args[3]

    def size_of(type_id, seen=()):
        if type_id in seen: return None
        kind, data = types.get(type_id, (None, [])); seen = seen + (type_id,)
        if kind in (TYPE_INT, TYPE_FLOAT): return data[0] // 8 if data and data[0] in (8, 16, 32, 64) else None
        if kind == TYPE_VECTOR:
            element = size_of(data[0], seen)
            return None if element is None else element * data[1]
        if kind == TYPE_ARRAY:
            stride, length = decorations.get((type_id, ARRAY_STRIDE)), constants.get(data[1])
            return stride * length if type(stride) is int and length is not None and 0 <= length <= 65536 else None
        if kind == TYPE_STRUCT:
            end = 0
            for index, member in enumerate(data):
                offset, size = member_offsets.get((type_id, index)), size_of(member, seen)
                if offset is None or size is None: return None
                end = max(end, offset + size)
            return end
        return None  # Matrix/member stride and runtime aggregates require explicit further support.

    def pointee(pointer_type):
        kind, data = types.get(pointer_type, (None, []))
        return data[1] if kind == TYPE_POINTER and len(data) == 2 and data[0] == UNIFORM else None

    pointer_results = {}
    for opcode, args in ops:
        if opcode in (43, 45, 46, 55, 57, 59, 65, 66, 67, 70, 83, 120, 124, 169, 245) and len(args) >= 2:
            if types.get(args[0], (None, []))[0] == TYPE_POINTER:
                pointer_results[args[1]] = args[0]
    targets = set()
    pointers = {}
    for opcode, args in ops:
        if opcode == VARIABLE and len(args) >= 3 and args[2] == UNIFORM:
            pointers[args[1]] = (args[1], pointee(args[0]), 0, True)
            if decorations.get((args[1], BINDING)) == binding and decorations.get((args[1], DESCRIPTOR_SET), 0) == descriptor_set:
                targets.add(args[1])
    static_loads, dynamic_loads, unknown_loads, outside_loads, unsupported_reads = 0, 0, 0, 0, 0
    read_words = set()
    for opcode, args in ops:
        if opcode in (ACCESS_CHAIN, INBOUNDS_ACCESS_CHAIN) and len(args) >= 3:
            if pointee(args[0]) is None:
                pointers[args[1]] = (None, None, None, False)
                continue
            root, current, offset, eligible = pointers.get(args[2], (None, None, None, True))
            indices = args[3:]
            # An array of uniform Blocks is a descriptor array, not byte-strided data inside one buffer.
            kind, data = types.get(current, (None, []))
            if kind == TYPE_ARRAY and data and decorations.get((data[0], BLOCK)):
                selected = constants.get(indices[0]) if indices else None
                eligible = eligible and (selected == array_element or selected is None)
                if selected is None: offset = None
                current = data[0]; indices = indices[1:]
            for index_id in indices:
                index = constants.get(index_id)
                kind, data = types.get(current, (None, []))
                if index is None or index < 0 or offset is None:
                    offset = None; break
                if kind == TYPE_STRUCT:
                    member_offset = member_offsets.get((current, index))
                    if index >= len(data) or member_offset is None: offset = None; break
                    offset += member_offset; current = data[index]
                elif kind in (TYPE_ARRAY, TYPE_RUNTIME_ARRAY):
                    stride = decorations.get((current, ARRAY_STRIDE))
                    length = constants.get(data[1]) if kind == TYPE_ARRAY else None
                    if type(stride) is not int or (length is not None and index >= length): offset = None; break
                    offset += stride * index; current = data[0]
                elif kind == TYPE_VECTOR:
                    scalar = size_of(data[0])
                    if scalar is None or index >= data[1]: offset = None; break
                    offset += scalar * index; current = data[0]
                else:
                    offset = None; break
            pointers[args[1]] = (root, current, offset, eligible)
        elif opcode == COPY_OBJECT and len(args) >= 3 and pointee(args[0]) is not None:
            pointers[args[1]] = pointers.get(args[2], (None, pointee(args[0]), None, True))
        elif opcode in (PTR_ACCESS_CHAIN, INBOUNDS_PTR_ACCESS_CHAIN) and len(args) >= 3:
            root, _, _, eligible = pointers.get(args[2], (None, None, None, True))
            pointers[args[1]] = (root, pointee(args[0]), None, eligible)
        elif opcode == LOAD and len(args) >= 3:
            if args[2] not in pointer_results:
                raise ValueError('Load references an unknown pointer ID')
            if pointee(pointer_results[args[2]]) is None:
                continue
            root, _, offset, eligible = pointers.get(args[2], (None, None, None, True))
            if not eligible: continue
            if root is None:
                # Only count unresolved Uniform pointers, not unrelated storage/image loads.
                unknown_loads += 1
                continue
            if root not in targets: continue
            size = size_of(args[0])
            if offset is None or size is None:
                dynamic_loads += 1; continue
            if offset < 0 or size <= 0 or offset + size > byte_limit:
                outside_loads += 1; continue
            static_loads += 1
            read_words.update(range(offset // 4 * 4, (offset + size + 3) // 4 * 4, 4))
        elif opcode in (63, 64) and len(args) >= 2:  # OpCopyMemory[ Sized ]
            root = pointers.get(args[1], (None, None, None, True))[0]
            if root in targets: unsupported_reads += 1
        elif len(args) >= 2 and opcode not in (VARIABLE, DECORATE, MEMBER_DECORATE) and pointee(args[0]) is not None:
            # Phi/select/function pointer values remain unresolved rather than borrowing another binding.
            pointers[args[1]] = (None, pointee(args[0]), None, True)
    regions = {'view': (0, 0x30), 'view_projection': (0x30, 0x70), 'projection': (0x70, 0xb0),
               'inverse_view_candidate': (0xb0, 0xe0), 'projection_like_0x1c0': (0x1c0, 0x200)}
    return {'schema': 1, 'binding': binding, 'descriptor_set': descriptor_set, 'array_element': array_element,
            'matching_uniform_variables': len(targets), 'static_loads': static_loads,
            'dynamic_or_unsupported_target_loads': dynamic_loads, 'unknown_uniform_pointer_loads': unknown_loads,
            'out_of_range_target_loads': outside_loads, 'unsupported_target_memory_operations': unsupported_reads,
            'static_read_word_offsets': sorted(read_words),
            'camera_regions_read': [name for name, (start, end) in regions.items() if any(start <= off < end for off in read_words)],
            'outside_captured_camera_block': sum(off >= 0x230 for off in read_words),
            'static_subset_resolved': bool(targets) and not (dynamic_loads or unknown_loads or outside_loads or unsupported_reads),
            'safe_to_patch': False, 'scope': 'Conservative static load footprint, not complete shader semantic proof'}


def read_module(path):
    path = Path(path)
    length = path.stat().st_size
    if length > 8 * 1024 * 1024 or length % 4:
        raise ValueError('Invalid bounded SPIR-V byte length')
    data = path.read_bytes()
    return list(struct.unpack('<' + 'I'*(len(data)//4), data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module', type=Path)
    parser.add_argument('--binding', type=int, required=True)
    parser.add_argument('--set', type=int, default=0, dest='descriptor_set')
    parser.add_argument('--array-element', type=int, default=0)
    parser.add_argument('--byte-limit', type=int, default=65536)
    args = parser.parse_args()
    try:
        result = analyze(read_module(args.module), args.binding, args.descriptor_set, args.array_element, args.byte_limit)
    except (OSError, ValueError, IndexError, TypeError) as error:
        parser.exit(2, str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
