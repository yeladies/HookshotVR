"""Inspect a private incomplete image-lineage capture; never executes a replay."""
import argparse
import json
import importlib.util
from pathlib import Path


def analyze(data):
    if data.get('schema') != 1 or data.get('replay_ready') is not False or data.get('coverage_complete') is not False:
        raise ValueError('Expected an explicitly incomplete diagnostic capture')
    events = data.get('events')
    if not isinstance(events, list) or len(events) > 4096:
        raise ValueError('Invalid event list')
    writers = {}
    buffer_writers = {}
    latest = {}
    dependencies = []
    imported = []
    imported_buffers = []
    for index, event in enumerate(events):
        if event.get('kind') not in (0, 1, 2):
            raise ValueError('Unknown work kind')
        if not all(isinstance(event.get(key), bool) for key in ('seed', 'dependent', 'opaque_buffers')):
            raise ValueError('Invalid event flags')
        if not isinstance(event.get('reads'), list) or not isinstance(event.get('writes'), list):
            raise ValueError('Missing resource versions')
        if len(event['reads']) > 128 or len(event['writes']) > 64:
            raise ValueError('Resource list exceeds observer bounds')
        parents = set()
        roots = set()
        for raw in event['reads']:
            image, version = resource(raw)
            if not version:
                roots.add(image)
            elif (image, version) not in writers:
                raise ValueError('Read names an unknown or future image version')
            else:
                parents.add(writers[(image, version)])
        buffer_roots = set()
        buffer_reads = event.get('buffer_reads', [])
        buffer_writes = event.get('buffer_writes', [])
        if not isinstance(buffer_reads, list) or not isinstance(buffer_writes, list) or len(buffer_reads) > 512 or len(buffer_writes) > 64:
            raise ValueError('Invalid buffer dependency lists')
        for raw in buffer_reads:
            obj, offset, size, version = buffer_resource(raw)
            if not version:
                buffer_roots.add((obj, offset, size))
            elif version not in buffer_writers or buffer_writers[version][0] != (obj, offset, size):
                raise ValueError('Unknown or future buffer version')
            else:
                parents.add(buffer_writers[version][1])
        imported_buffers.append(buffer_roots)
        dependencies.append(parents)
        imported.append(roots)
        outputs = set()
        for raw in event['writes']:
            image, version = resource(raw)
            if not version or version != latest.get(image, 0) + 1 or image in outputs:
                raise ValueError('Duplicate or nonconsecutive image write version')
            outputs.add(image)
            writers[(image, version)] = index
            latest[image] = version
        for raw in buffer_writes:
            obj, offset, size, version = buffer_resource(raw)
            if version != len(buffer_writers) + 1:
                raise ValueError('Duplicate or nonconsecutive buffer version')
            buffer_writers[version] = ((obj, offset, size), index)
    present = data.get('present_image')
    if present not in latest:
        raise ValueError('Presented image has no observed producer')
    pending = [writers[(present, latest[present])]]
    selected = set()
    roots = set()
    buffer_roots = set()
    while pending:
        index = pending.pop()
        if index in selected:
            continue
        selected.add(index)
        roots.update(imported[index])
        buffer_roots.update(imported_buffers[index])
        pending.extend(dependencies[index] - selected)
    counts = {'graphics': 0, 'compute': 0, 'replacement': 0}
    for index in selected:
        counts[('graphics', 'compute', 'replacement')[events[index]['kind']]] += 1
    per_eye = {'graphics': 0, 'compute': 0, 'replacement': 0}
    private_images = set()
    private_buffers = set()
    for index in selected:
        event = events[index]
        if event['dependent']:
            per_eye[('graphics', 'compute', 'replacement')[event['kind']]] += 1
            private_images.update(raw[0] for raw in event['writes'])
            private_buffers.update(tuple(raw[:3]) for raw in event.get('buffer_writes', []))
    private_reads = set()
    for index in selected:
        if events[index]['dependent']:
            private_reads.update(tuple(raw[:3]) for raw in events[index].get('buffer_reads', []))
    descriptions = {}
    resources = data.get('resources', [])
    if not isinstance(resources, list) or len(resources) > 2048:
        raise ValueError('Invalid image inventory')
    for description in resources:
        identity = description.get('image')
        keys = ('image', 'allocation_bytes', 'format', 'width', 'height', 'depth', 'levels', 'layers', 'samples', 'aspect', 'type')
        if (any(type(description.get(key)) is not int or description[key] < 0 for key in keys) or
                not identity or identity in descriptions or not isinstance(description.get('sparse'), bool)):
            raise ValueError('Invalid or duplicate image description')
        if any(not description[key] for key in ('format', 'width', 'height', 'depth', 'levels', 'layers', 'samples')):
            raise ValueError('Empty image shape')
        descriptions[identity] = description
    described_private = [descriptions[identity] for identity in private_images if identity in descriptions]
    image_bytes = sum(description['allocation_bytes'] for description in described_private)
    alias_plan = buffer_alias_closure(private_buffers, private_reads)
    spec = importlib.util.spec_from_file_location('same_state_plan', Path(__file__).with_name('same_state_plan.py'))
    planner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(planner)
    same_state = planner.plan(events, selected, dependencies, imported, imported_buffers, descriptions)
    return {'schema': 1, 'same_state_plan': same_state, 'observed_events': len(events), 'present_slice_events': len(selected),
            'present_slice_kinds': counts,
            'camera_seed_events_in_slice': sum(events[i]['seed'] for i in selected),
            'camera_dependent_events_in_slice': sum(events[i]['dependent'] for i in selected),
            'opaque_buffer_events_in_slice': sum(events[i]['opaque_buffers'] for i in selected),
            'imported_image_roots_in_slice': len(roots),
            'imported_buffer_ranges_in_slice': len(buffer_roots),
            'observed_buffer_writes': len(buffer_writers), 'private_buffer_range_candidates': len(private_buffers),
            'per_eye_work_candidates': per_eye, 'private_image_candidates': len(private_images),
            'private_buffer_alias_groups': len(alias_plan),
            'private_buffer_bytes_per_eye_reference': sum(end - start for _, start, end in alias_plan),
            'private_buffers_require_initial_copy': bool(alias_plan),
            'missing_private_image_descriptions': len(private_images) - len(described_private),
            'private_image_allocation_reference_bytes_per_eye': image_bytes,
            'private_image_allocation_reference_bytes_two_eyes': 2 * image_bytes,
            'private_image_zero_size_references': sum(not description['allocation_bytes'] for description in described_private),
            'private_image_sparse_candidates': sum(description['sparse'] for description in described_private),
            'coverage_complete': False, 'replay_ready': False,
            'required_review': ['untracked buffers and indirect arguments', 'image copies, clears and aliasing',
                                'mip/layer/aspect ranges and partial writes', 'prior-frame temporal resources',
                                'camera culling, UI and per-eye private destinations']}


def buffer_alias_closure(writes, reads):
    """Expand seed writes to complete overlapping descriptor ranges; no GPU allocation."""
    def merged(spans):
        result = []
        for obj, start, end in sorted(spans):
            if result and result[-1][0] == obj and start <= result[-1][2]:
                result[-1] = (obj, result[-1][1], max(end, result[-1][2]))
            else:
                result.append((obj, start, end))
        return result
    spans = merged((obj, offset, offset + size) for obj, offset, size in writes)
    remaining = set(reads)
    while True:
        touching = {raw for raw in remaining if any(obj == raw[0] and start < raw[1] + raw[2] and raw[1] < end
                                                   for obj, start, end in spans)}
        if not touching:
            return spans
        spans = merged(spans + [(obj, offset, offset + size) for obj, offset, size in touching])
        remaining -= touching

def buffer_resource(raw):
    if (not isinstance(raw, list) or len(raw) != 4 or any(type(value) is not int for value in raw) or
            raw[0] <= 0 or raw[1] < 0 or raw[2] <= 0 or raw[3] < 0 or raw[1] + raw[2] > (1 << 64) - 1):
        raise ValueError('Invalid buffer range/version identity')
    return raw


def resource(raw):
    if (not isinstance(raw, list) or len(raw) != 2 or
            any(type(value) is not int for value in raw) or raw[0] <= 0 or raw[1] < 0):
        raise ValueError('Invalid image/version identity')
    return raw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    try:
        if args.capture.stat().st_size > 64 * 1024 * 1024:
            raise ValueError('Capture exceeds the diagnostic size limit')
        result = analyze(json.loads(args.capture.read_text(encoding='utf-8-sig')))
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
