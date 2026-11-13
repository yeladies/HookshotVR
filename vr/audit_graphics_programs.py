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

def analyze_directory(directory,grammar,require_association=False):
    root=Path(directory).resolve();associated=False
    if require_association:
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
        report["module_sha256"]=hashlib.sha256(data).hexdigest();modules[digest]=report;return report
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
                "shader_subset_candidate":subset,"effects":report})
        if stages:
            for binding in record["bindings"]:
                stage=seen.get(binding["vk_stage"])
                if not stage or stage["nvn_stage"]!=binding["nvn_stage"] or stage.get("module")!=binding["module"]:raise ValueError("Binding does not match stage inventory")
        primary=any(binding["primary_camera_binding"] for binding in record["bindings"])
        camera_regions=set();vertex_transform=False
        for binding in record["bindings"]:
            if not binding["primary_camera_binding"] or binding["module"] not in modules:continue
            key=(binding["module"],binding["binding"],binding["array_element"],binding["range_bytes"])
            if key not in queries:
                try:queries[key]=bindings.reader.analyze(bindings.reader.read_module(root/(binding["module"]+".spv")),binding["binding"],0,binding["array_element"],binding["range_bytes"])
                except (OSError,ValueError,IndexError,TypeError):queries[key]={"static_read_word_offsets":[]}
            words=queries[key].get("static_read_word_offsets",[])
            ranges=(("view",0,48),("view_projection",48,112),("projection",112,176),("inverse_view",176,224))
            observed={name for name,first,end in ranges if any(first<=word<end for word in words)}
            camera_regions.update(observed)
            if binding["vk_stage"]==1 and observed.intersection({"view","view_projection","inverse_view"}):vertex_transform=True
        candidate=complete and available and readonly and set(seen)=={1,16} and primary
        reports.append({"program":record["program"],"stage_inventory_declared_complete":complete,
            "all_declared_stage_files_available":available,"primary_binding_observed":primary,
            "shader_subset_candidate":candidate,"observed_primary_byte_regions":sorted(camera_regions),
            "vertex_camera_transform_reads_observed":vertex_transform,"stages":stage_reports,"safe_to_replay":False})
    return {"schema":1,"observed_programs":len(reports),"complete_stage_inventories":sum(r["stage_inventory_declared_complete"] for r in reports),
        "programs_with_all_stage_files":sum(r["all_declared_stage_files_available"] for r in reports),
        "shader_subset_candidates":sum(r["shader_subset_candidate"] for r in reports),
        "shader_candidates_with_vertex_transform_reads":sum(r["shader_subset_candidate"] and r["vertex_camera_transform_reads_observed"] for r in reports),
        "unique_stage_modules":len(modules),
        "missing_stage_modules":missing,"same_process_manifest_verified":associated,"uniform_binding_summary":uniform_summary,
        "pipeline_state_coverage_complete":False,"resource_alias_coverage_complete":False,"safe_to_replay":False,
        "programs":reports}

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directory",type=Path)
    parser.add_argument("--grammar",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--require-associated-lineage",action="store_true");args=parser.parse_args()
    if args.grammar.stat().st_size>8*1024*1024:parser.error("Grammar size exceeds bound")
    try:report=analyze_directory(args.directory,json.loads(args.grammar.read_text(encoding="utf-8")),args.require_associated_lineage)
    except (ValueError,KeyError,TypeError,OSError) as error:parser.error(str(error))
    args.output.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
if __name__=="__main__":main()
