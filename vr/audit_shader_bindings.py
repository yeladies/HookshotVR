# SPDX-License-Identifier: GPL-3.0-or-later
"""Summarize private native module/binding observations without exposing shader code."""
import argparse
import importlib.util
import json
from pathlib import Path
import re

spec = importlib.util.spec_from_file_location('uniform_reader', Path(__file__).with_name('audit_spirv_uniforms.py'))
reader = importlib.util.module_from_spec(spec); spec.loader.exec_module(reader)


def analyze_directory(directory):
    root = Path(directory).resolve()
    metadata = root / 'program-bindings.jsonl'
    if metadata.stat().st_size > 4 * 1024 * 1024:
        raise ValueError('Binding metadata exceeds its limit')
    records = [json.loads(line) for line in metadata.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    if not records or len(records) > 64:
        raise ValueError('Expected one to 64 observed programs')
    programs, queries = set(), {}
    stage_regions = {}
    primary_bindings = missing = unresolved = static_queries = resolved = outside = 0
    for record in records:
        program = record.get('program')
        if record.get('schema') != 1 or type(program) is not int or not 0 < program < (1 << 64) or program in programs:
            raise ValueError('Invalid or repeated program observation')
        programs.add(program)
        bindings = record.get('bindings')
        if not isinstance(bindings, list) or len(bindings) > 128:
            raise ValueError('Invalid bounded binding list')
        for binding in bindings:
            if not isinstance(binding.get('primary_camera_binding'), bool):
                raise ValueError('Missing camera-source provenance flag')
            fields = ('nvn_stage', 'vk_stage', 'binding', 'array_element', 'bank', 'range_bytes')
            if any(type(binding.get(field)) is not int or binding[field] < 0 for field in fields):
                raise ValueError('Invalid binding integers')
            if not binding['primary_camera_binding']:
                continue
            primary_bindings += 1
            digest = binding.get('module')
            if digest is None:
                missing += 1;continue
            if not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{16}', digest):
                raise ValueError('Invalid module fingerprint')
            path = root / (digest + '.spv')
            if path.is_symlink() or path.resolve().parent != root:
                raise ValueError('Module path escapes the private audit directory')
            key = (digest, binding['binding'], binding['array_element'], binding['range_bytes'])
            if key not in queries:
                try:
                    result = reader.analyze(reader.read_module(path), binding['binding'], 0, binding['array_element'], binding['range_bytes'])
                except (OSError, ValueError, IndexError, TypeError) as error:
                    result = {'error': str(error), 'camera_regions_read': [], 'static_loads': 0, 'static_subset_resolved': False}
                queries[key] = result
            result = queries[key]
            stage_regions.setdefault(str(binding['nvn_stage']), set()).update(result['camera_regions_read'])
    for result in queries.values():
        if result.get('static_loads', 0): static_queries += 1
        if result.get('static_subset_resolved'): resolved += 1
        else: unresolved += 1
        outside += result.get('outside_captured_camera_block', 0)
    return {'schema': 1, 'observed_programs': len(programs), 'primary_camera_bindings': primary_bindings,
            'unique_module_binding_queries': len(queries), 'queries_with_static_reads': static_queries,
            'resolved_static_subsets': resolved, 'unresolved_queries': unresolved, 'missing_primary_modules': missing,
            'read_words_outside_captured_block': outside,
            'camera_regions_by_nvn_stage': {stage: sorted(regions) for stage, regions in sorted(stage_regions.items())},
            'query_errors': sorted({result['error'] for result in queries.values() if 'error' in result}),
            'safe_to_patch': False, 'complete_shader_semantic_proof': False,
            'scope': 'Representative source-checked bindings and conservative static module reads only'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        result = analyze_directory(args.directory)
    except (OSError, ValueError, IndexError, TypeError, KeyError) as error:
        parser.exit(2, str(error) + '\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__': main()
