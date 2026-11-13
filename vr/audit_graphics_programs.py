# SPDX-License-Identifier: GPL-3.0-or-later
"""Join complete native stage inventories to conservative shader effect observations."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct

def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
_here=Path(__file__).resolve().parent
effects=load("vr_program_effects",_here/"audit_graphics_effects.py")
bindings=load("vr_program_bindings",_here/"audit_shader_bindings.py")
WRITES=("uses_global_memory","writes_storage_buffers","writes_storage_images","writes_texel_buffers")

def fingerprint(data):
    value=14695981039346656037
    for byte in data:value=((value^byte)*1099511628211)&((1<<64)-1)
    return f"{value:016x}"

def baseline_preflight(directory,programs):
    root=Path(directory).resolve();path=root/"baseline-preflight.jsonl"
    if not path.exists():return None
    if path.is_symlink() or path.resolve().parent!=root or path.stat().st_size>512*1024:raise ValueError("Invalid baseline preflight path/budget")
    association=json.loads((root/"capture-association.json").read_text(encoding="utf-8-sig"))
    data=path.read_bytes()
    digest=association.get("baseline_preflight_sha256")
    if association.get("same_process_capture") is not True or not isinstance(digest,str) or not re.fullmatch("[0-9a-fA-F]{64}",digest) or digest.lower()!=hashlib.sha256(data).hexdigest():raise ValueError("Baseline preflight capture association mismatch")
    records=[json.loads(line) for line in data.decode("utf-8-sig").splitlines() if line.strip()]
    if len(records)>256:raise ValueError("Baseline preflight record budget exceeded")
    known={p["program"]:p for p in programs};eligible=set();reasons={}
    for record in records:
        if record.get("schema")!=1 or record.get("extra_draw_recorded") is not False or record.get("safe_to_replay") is not False or record.get("shader_effects_reviewed") is not False:raise ValueError("Not a read-only baseline preflight")
        key=record.get("program");reason=record.get("reason")
        if not isinstance(key,str) or not re.fullmatch("[0-9a-f]{1,16}",key) or not isinstance(reason,str) or not re.fullmatch("[a-z_]{1,80}",reason):raise ValueError("Invalid baseline preflight identity")
        for field in ("recording","frame","width","height","uniform_bytes","camera_bindings","read_images"):
            if type(record.get(field)) is not int or not 0<=record[field]<(1<<64):raise ValueError("Invalid baseline preflight measurement")
        reasons[reason]=reasons.get(reason,0)+1
        program=known.get(int(key,16))
        if reason=="live_state_candidate_requires_shader_review" and program and program["shader_subset_candidate"] and program["vertex_camera_transform_reads_observed"]:
            if not record["recording"] or not 0<record["width"]<=4096 or not 0<record["height"]<=4096 or not 0<record["uniform_bytes"]<=65536 or record["uniform_bytes"]%4 or not 0<record["camera_bindings"]<=32:raise ValueError("Invalid live-state candidate bounds")
            eligible.add(program["program"])
    return {"records":len(records),"reasons":reasons,"shader_and_live_state_candidates":sorted(eligible),"capture_link_hash_verified":True,"safe_to_replay":False}

def eye_patch_footprint(footprints):
    """Compare observed reads with the existing private-copy matrix patch extent.

    A known byte range is only transport coverage, never shader semantic proof.
    Unknown fields stay byte-exact, but may still be camera-dependent.
    """
    rows=[];resolved=bool(footprints);matrix_reads=0;review=set()
    for binding,query in footprints:
        words=query.get("static_read_word_offsets",[])
        if not isinstance(words,list) or len(words)>16384 or any(type(n) is not int or n<0 or n%4 or n>=binding["range_bytes"] for n in words):
            raise ValueError("Invalid bounded camera read footprint")
        complete=query.get("static_subset_resolved") is True
        resolved &= complete
        covered=sorted(set(n for n in words if n<0xe0))
        unknown=sorted(set(n for n in words if n>=0xe0))
        matrix_reads+=len(covered);review.update(unknown)
        rows.append({"vk_stage":binding["vk_stage"],"binding":binding["binding"],"array_element":binding["array_element"],
            "range_bytes":binding["range_bytes"],"static_subset_resolved":complete,
            "matrix_patch_read_byte_offsets":covered,"preserved_fields_requiring_review":unknown})
    status=("unresolved_camera_reads" if not resolved else "unreviewed_preserved_fields" if review else
            "matrix_footprint_only" if matrix_reads else "no_observed_matrix_reads")
    return {"status":status,"primary_binding_footprints":rows,"all_primary_footprints_resolved":resolved,
        "matrix_read_words":matrix_reads,"preserved_read_byte_offsets_requiring_review":sorted(review),
        "matrix_patch_extent_bytes":0xe0,"shader_semantics_confirmed":False,"safe_to_patch":False,
        "full_scene_stereo":False}


def analyze_directory(directory,grammar,require_association=False):
    root=Path(directory).resolve();associated=False
    if require_association or (root/"baseline-preflight.jsonl").exists():
        link=load("vr_program_association",_here/"analyze_stereo_candidates.py");link.association(root);associated=True
    uniform_summary=bindings.analyze_directory(root)
    records=[json.loads(line) for line in (root/"program-bindings.jsonl").read_text(encoding="utf-8-sig").splitlines() if line.strip()]
    modules={};queries={};total_bytes=0;missing=0;reports=[]
    def module(digest):
        nonlocal total_bytes,missing
        if digest is None:missing+=1;return None
        if not isinstance(digest,str) or not re.fullmatch("[0-9a-f]{16}",digest):raise ValueError("Invalid stage module fingerprint")
        if digest in modules:return modules[digest]
        path=root/(digest+".spv")
        if path.is_symlink() or path.resolve().parent!=root:raise ValueError("Stage module escapes capture")
        if not path.is_file():missing+=1;return None
        size=path.stat().st_size
        if size>8*1024*1024 or size%4 or total_bytes+size>64*1024*1024:raise ValueError("Stage module byte budget exceeded")
        data=path.read_bytes()
        if fingerprint(data)!=digest:raise ValueError("Stage module content does not match exported fingerprint")
        total_bytes+=len(data)
        report=effects.analyze(struct.unpack("<"+"I"*(len(data)//4),data),grammar)
        report["module_sha256"]=hashlib.sha256(data).hexdigest();report["module_words"]=len(data)//4;modules[digest]=report;return report
    for record in records:
        complete=record.get("stage_inventory_complete",False);stages=record.get("stages",[])
        if type(complete) is not bool or not isinstance(stages,list) or len(stages)>8 or (complete and not stages):raise ValueError("Invalid stage inventory")
        seen={};available=bool(stages);readonly=bool(stages);stage_reports=[]
        for stage in stages:
            vk,ns=stage.get("vk_stage"),stage.get("nvn_stage")
            if type(vk) is not int or vk not in (1,2,4,8,16,32,64,128) or vk in seen or type(ns) is not int or not 0<=ns<8:raise ValueError("Invalid or duplicate native stage")
            if any(type(stage.get(flag)) is not bool for flag in WRITES):raise ValueError("Missing typed native effect flags")
            seen[vk]=stage
            report=module(stage.get("module"));available&=report is not None
            expected={1:0,16:4}.get(vk)
            subset=bool(report and report["entry_models"]==[expected] and report["supported_read_only_subset"] and not any(stage[flag] for flag in WRITES))
            readonly&=subset
            stage_reports.append({"vk_stage":vk,"nvn_stage":ns,"module_available":report is not None,
                "shader_subset_candidate":subset,"module_fingerprint":stage.get("module"),"effects":report})
        if stages:
            for binding in record["bindings"]:
                stage=seen.get(binding["vk_stage"])
                if not stage or stage["nvn_stage"]!=binding["nvn_stage"] or stage.get("module")!=binding["module"]:raise ValueError("Binding does not match stage inventory")
        primary=any(binding["primary_camera_binding"] for binding in record["bindings"])
        camera_regions=set();vertex_transform=False;patch_footprints=[]
        for binding in record["bindings"]:
            if not binding["primary_camera_binding"]:continue
            if binding["module"] not in modules:
                patch_footprints.append((binding,{}));continue
            key=(binding["module"],binding["binding"],binding["array_element"],binding["range_bytes"])
            if key not in queries:
                try:queries[key]=bindings.reader.analyze(bindings.reader.read_module(root/(binding["module"]+".spv")),binding["binding"],0,binding["array_element"],binding["range_bytes"])
                except (OSError,ValueError,IndexError,TypeError):queries[key]={"static_read_word_offsets":[]}
            patch_footprints.append((binding,queries[key]))
            words=queries[key].get("static_read_word_offsets",[])
            ranges=(("view",0,48),("view_projection",48,112),("projection",112,176),("inverse_view",176,224))
            observed={name for name,first,end in ranges if any(first<=word<end for word in words)}
            camera_regions.update(observed)
            if binding["vk_stage"]==1 and observed.intersection({"view","view_projection","inverse_view"}):vertex_transform=True
        candidate=complete and available and readonly and set(seen)=={1,16} and primary
        reports.append({"program":record["program"],"stage_inventory_declared_complete":complete,
            "all_declared_stage_files_available":available,"primary_binding_observed":primary,
            "shader_subset_candidate":candidate,"observed_primary_byte_regions":sorted(camera_regions),
            "vertex_camera_transform_reads_observed":vertex_transform,"stages":stage_reports,
            "eye_patch_preflight":eye_patch_footprint(patch_footprints),"safe_to_replay":False})
    return {"schema":1,"observed_programs":len(reports),"complete_stage_inventories":sum(r["stage_inventory_declared_complete"] for r in reports),
        "programs_with_all_stage_files":sum(r["all_declared_stage_files_available"] for r in reports),
        "shader_subset_candidates":sum(r["shader_subset_candidate"] for r in reports),
        "shader_candidates_with_vertex_transform_reads":sum(r["shader_subset_candidate"] and r["vertex_camera_transform_reads_observed"] for r in reports),
        "unique_stage_modules":len(modules),
        "missing_stage_modules":missing,"same_process_manifest_verified":associated,"uniform_binding_summary":uniform_summary,
        "pipeline_state_coverage_complete":False,"resource_alias_coverage_complete":False,"safe_to_replay":False,
        "baseline_preflight":baseline_preflight(root,reports),"programs":reports}

def baseline_selection_text(report,nonce,source,lens_shift=False):
    if not isinstance(nonce,str) or not re.fullmatch("[0-9a-f]{32}",nonce) or not isinstance(source,str) or not re.fullmatch("[0-9a-f]{8}",source):raise ValueError("Invalid baseline run/build identity")
    if report.get("safe_to_replay") is not False:raise ValueError("Expected diagnostic-only program report")
    selected=[];seen=set()
    for program in report.get("programs",[]):
        if program.get("shader_subset_candidate") is not True or program.get("vertex_camera_transform_reads_observed") is not True:continue
        if lens_shift:
            footprint=program.get("eye_patch_preflight") or {}
            if footprint.get("status")!="matrix_footprint_only" or not footprint.get("all_primary_footprints_resolved"):continue
            if any(row.get("vk_stage")!=1 and row.get("matrix_patch_read_byte_offsets") for row in footprint.get("primary_binding_footprints",[])):continue
        key=program.get("program");stages=program.get("stages",[])
        if type(key) is not int or not 0<key<(1<<64) or key in seen or len(stages)!=2:raise ValueError("Invalid baseline program identity")
        seen.add(key);mask=set();lines=[]
        for stage in stages:
            vk,ns,digest=stage.get("vk_stage"),stage.get("nvn_stage"),stage.get("module_fingerprint")
            effect=stage.get("effects") or {};words=effect.get("module_words")
            if type(vk) is not int or vk not in (1,16) or vk in mask or type(ns) is not int or not 0<=ns<8:raise ValueError("Invalid baseline stage")
            if stage.get("shader_subset_candidate") is not True or effect.get("supported_read_only_subset") is not True or effect.get("safe_to_replay") is not False:raise ValueError("Incomplete baseline shader evidence")
            if not isinstance(digest,str) or not re.fullmatch("[0-9a-f]{16}",digest) or type(words) is not int or not 5<=words<=2*1024*1024:raise ValueError("Invalid baseline module identity")
            mask.add(vk);lines.append(f"{ns} {vk} {digest} {words}")
        if mask!={1,16}:raise ValueError("Baseline requires a vertex/fragment pair")
        selected.append(f"{key:016x} 2\n"+"\n".join(lines))
        if len(selected)==32:break
    if not selected:raise ValueError("No inspected baseline shader candidates yet")
    magic="HSVR_LENS_SHIFT_DRAW_V1" if lens_shift else "HSVR_BASELINE_DRAW_V1"
    text=f"{magic}\n{nonce} {source} {len(selected)}\n"+"\n".join(selected)+"\n"
    if len(text.encode("ascii"))>8192:raise ValueError("Baseline selection exceeds native bound")
    return text


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path)
    parser.add_argument("--grammar",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--require-associated-lineage",action="store_true")
    parser.add_argument("--baseline-selection",type=Path);parser.add_argument("--nonce");parser.add_argument("--source-fingerprint")
    parser.add_argument("--lens-shift-selection",action="store_true",help="Select only resolved matrix-prefix vertex reads for the isolated lens-shift experiment")
    args=parser.parse_args()
    if args.grammar.stat().st_size>8*1024*1024:parser.error("Grammar size exceeds bound")
    try:report=analyze_directory(args.directory,json.loads(args.grammar.read_text(encoding="utf-8")),args.require_associated_lineage)
    except (ValueError,KeyError,TypeError,OSError) as error:parser.error(str(error))
    args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    if args.baseline_selection:
        try:text=baseline_selection_text(report,args.nonce,args.source_fingerprint,args.lens_shift_selection)
        except (ValueError,TypeError) as error:parser.error(str(error))
        temporary=args.baseline_selection.with_name(args.baseline_selection.name+"."+args.nonce+".tmp")
        temporary.write_text(text,encoding="ascii",newline="\n");temporary.replace(args.baseline_selection)
if __name__=="__main__":main()
