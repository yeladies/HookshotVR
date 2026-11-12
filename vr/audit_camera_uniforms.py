"""Read private camera blocks and find algebraic layout candidates; never patch memory."""
import argparse
import json
import math
from pathlib import Path


def inverse(matrix):
    rows = [[float(matrix[4*r+c]) for c in range(4)] + [float(r == c) for c in range(4)] for r in range(4)]
    for column in range(4):
        pivot = max(range(column, 4), key=lambda row: abs(rows[row][column]))
        if abs(rows[pivot][column]) < 1e-12:
            raise ValueError('Singular camera matrix')
        rows[column], rows[pivot] = rows[pivot], rows[column]
        scale = rows[column][column]
        rows[column] = [value / scale for value in rows[column]]
        for row in range(4):
            if row != column:
                factor = rows[row][column]
                rows[row] = [a - factor*b for a, b in zip(rows[row], rows[column])]
    return [rows[r][4+c] for r in range(4) for c in range(4)]


def multiply(a, b):
    return [sum(a[4*r+k]*b[4*k+c] for k in range(4)) for r in range(4) for c in range(4)]


def close(a, b, absolute=0.02, relative=0.0001):
    return len(a) == len(b) and all(abs(x-y) <= absolute + abs(y)*relative for x, y in zip(a, b))


def audit(samples):
    if not 1 <= len(samples) <= 8:
        raise ValueError('Expected one to eight private samples')
    common = None
    views = []
    for sample in samples:
        values = sample.get('floats')
        if (sample.get('schema') != 1 or not isinstance(values, list) or len(values) != 0x230//4 or
                any(type(value) not in (int, float) or not math.isfinite(value) for value in values)):
            raise ValueError('Invalid finite uniform block')
        if type(sample.get('camera_revision')) is not int or sample['camera_revision'] <= 0:
            raise ValueError('Missing accepted camera provenance')
        view = values[:12] + [0, 0, 0, 1]
        projection = values[0x70//4:0x70//4+16]
        vp = values[0x30//4:0x30//4+16]
        if not close(vp, multiply(projection, view)):
            raise ValueError('Incoherent submitted view/projection block')
        iv = inverse(view)
        references = {'view_3x4': view[:12], 'view_projection_4x4': vp, 'projection_4x4': projection,
                      'inverse_view_4x4': iv, 'inverse_view_3x4': iv[:12],
                      'inverse_view_projection_4x4': inverse(vp), 'inverse_projection_4x4': inverse(projection)}
        found = {name: {offset*4 for offset in range(0, len(values)-len(reference)+1, 4)
                        if close(values[offset:offset+len(reference)], reference, 0.0002, 0.000002)}
                 for name, reference in references.items()}
        common = found if common is None else {name: common[name] & found[name] for name in common}
        if not any(close(view, previous) for previous in views):
            views.append(view)
    return {'schema': 1, 'samples': len(samples), 'distinct_views': len(views),
            'multiple_view_confirmation': len(views) >= 3,
            'common_algebraic_candidates': {name: [hex(offset) for offset in sorted(offsets)] for name, offsets in common.items()},
            'safe_to_patch': False, 'shader_consumption_verified': False,
            'remaining': ['validate shader use and all camera-dependent fields', 'world scale and body anchor',
                          'per-eye private targets, buffers and temporal history']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    try:
        if args.capture.stat().st_size > 128 * 1024:
            raise ValueError('Capture exceeds bounded sample size')
        samples = [json.loads(line) for line in args.capture.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
        result = audit(samples)
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
