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
    observed_stages = set()
    primary_bindings = missing = unresolved = static_queries = resolved = outside = 0
    observed_bindings = missing_observed_modules = 0
    candidate_bindings = eligible_scans = incomplete_scans = 0
    candidate_reads = {"view":0,"projection":0,"vp":0}
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
            observed_bindings += 1
            observed_stages.add(binding['nvn_stage'])
            digest = binding.get('module')
            if digest is None:
                missing_observed_modules += 1
                if binding['primary_camera_binding']:
                    primary_bindings += 1; missing += 1
                continue
            if not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{16}', digest):
                raise ValueError('Invalid module fingerprint')
            path = root / (digest + '.spv')
            if path.is_symlink() or path.resolve().parent != root:
                raise ValueError('Module path escapes the private audit directory')
            if not path.is_file():
                missing_observed_modules += 1
                if binding['primary_camera_binding']:
                    primary_bindings += 1; missing += 1
                continue
            for flag in ('candidate_scan_eligible','candidate_scan_truncated'):
                if flag in binding and type(binding[flag]) is not bool:
                    raise ValueError('Invalid camera scan flag')
            ambiguity = binding.get('ambiguous_candidate_offsets',0)
            if type(ambiguity) is not int or not 0<=ambiguity<=4096:
                raise ValueError('Invalid camera scan ambiguity count')
            candidates = {}
            for kind, size in (("view",48),("projection",64),("vp",64)):
                values = binding.get('candidate_'+kind+'_offsets',[])
                if not isinstance(values,list) or len(values)>64 or any(type(v) is not int or v<0 or v%16 or v+size>binding['range_bytes'] for v in values):
                    raise ValueError('Invalid bounded camera candidate offsets')
                if values and not binding.get('candidate_scan_eligible',False):
                    raise ValueError('Candidate offsets require an eligible submitted scan')
                candidates[kind]=(values,size)
            eligible_scans += bool(binding.get('candidate_scan_eligible',False))
            incomplete_scans += bool(binding.get('candidate_scan_truncated',False) or binding.get('ambiguous_candidate_offsets',0))
            candidate = bool(candidates['view'][0] or candidates['vp'][0])
            if candidate: candidate_bindings += 1
            if not binding['primary_camera_binding'] and not candidate:
                continue
            if binding['primary_camera_binding']: primary_bindings += 1
            key = (digest, binding['binding'], binding['array_element'], binding['range_bytes'])
            if key not in queries:
                try:
                    result = reader.analyze(reader.read_module(path), binding['binding'], 0, binding['array_element'], binding['range_bytes'])
                except (OSError, ValueError, IndexError, TypeError) as error:
                    result = {'error': str(error), 'camera_regions_read': [], 'static_loads': 0, 'static_subset_resolved': False}
                queries[key] = result
            result = queries[key]
            if binding['primary_camera_binding']:
                stage_regions.setdefault(str(binding['nvn_stage']), set()).update(result['camera_regions_read'])
            reads = result.get('static_read_word_offsets',[])
            for kind,(offsets,size) in candidates.items():
                if any(start<=word<start+size for start in offsets for word in reads): candidate_reads[kind]+=1
    for result in queries.values():
        if result.get('static_loads', 0): static_queries += 1
        if result.get('static_subset_resolved'): resolved += 1
        else: unresolved += 1
        outside += result.get('outside_captured_camera_block', 0)
    return {'schema': 1, 'observed_programs': len(programs), 'primary_camera_bindings': primary_bindings,
            'eligible_candidate_scans': eligible_scans, 'ambiguous_or_truncated_candidate_scans': incomplete_scans,
            'candidate_camera_bindings': candidate_bindings, 'candidate_bindings_with_static_reads': candidate_reads,
            'candidate_camera_semantics_confirmed': False,
            'observed_nvn_stages': sorted(observed_stages), 'observed_bindings': observed_bindings, 'missing_observed_modules': missing_observed_modules,
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
