# SPDX-License-Identifier: GPL-3.0-or-later
"""Conservative graphics-shader effect inventory. Never patches or authorizes replay."""
import argparse
import collections
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
_spec=importlib.util.spec_from_file_location("vr_uniform_instruction_reader",Path(__file__).with_name("audit_spirv_uniforms.py"))
_reader=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(_reader)
instructions=_reader.instructions

LOCAL_WRITES={3,6,7} # Output, Private, Function. Pipeline transform feedback is a separate gate.
READ_CLASSES={0,1,2,3,6,7,9,12}
PURE_CLASSES={"Debug","Type-Declaration","Constant-Creation","Composite","Arithmetic",
              "Relational_and_Logical","Bit","Derivative","Primitive","Non-Uniform"}
MEMORY={"OpVariable","OpLoad","OpStore","OpCopyMemory","OpCopyMemorySized","OpAccessChain",
        "OpInBoundsAccessChain","OpPtrAccessChain","OpInBoundsPtrAccessChain","OpArrayLength",
        "OpPtrEqual","OpPtrNotEqual","OpPtrDiff"}
CONTROL={"OpPhi","OpLoopMerge","OpSelectionMerge","OpLabel","OpBranch","OpBranchConditional",
         "OpSwitch","OpKill","OpReturn","OpReturnValue","OpUnreachable","OpLifetimeStart","OpLifetimeStop"}
FUNCTIONS={"OpFunction","OpFunctionParameter","OpFunctionEnd","OpFunctionCall"}


def analyze(words,grammar):
    if len(words)>2*1024*1024:raise ValueError("Module exceeds 8 MiB")
    definitions={item["opcode"]:item for item in grammar["instructions"]}
    ops=list(instructions(words));reasons=collections.Counter();pointer_types={};result_types={};ids=set()
    imports={};functions=set();bodies=set();entry_models=[];entry_functions=[];calls=[];models=[]
    writes=collections.Counter();reads=collections.Counter();current=None
    def reject(reason):reasons[reason]+=1
    for opcode,args in ops:
        info=definitions.get(opcode)
        if info is None:reject("unknown_opcode");continue
        minimum=sum(0 if operand.get("quantifier") in ("?","*") else 2 if operand["kind"].startswith("Pair") else 1 for operand in info.get("operands",[]))
        if len(args)<minimum:raise ValueError("Missing instruction operands")
        name=info["opname"];kinds=[x["kind"] for x in info.get("operands",[])]
        if "IdResult" in kinds:
            position=kinds.index("IdResult")
            if position>1 or len(args)<=position:raise ValueError("Invalid result operand")
            result=args[position]
            if not 0<result<words[3] or result in ids:raise ValueError("Duplicate/out-of-bounds result ID")
            ids.add(result)
            if "IdResultType" in kinds:
                if kinds[0]!="IdResultType" or position!=1:raise ValueError("Unsupported result-type grammar")
                result_types[result]=args[0]
        if name=="OpTypePointer":
            if len(args)!=3:raise ValueError("Invalid pointer type")
            pointer_types[args[0]]=args[1]
        elif name=="OpExtInstImport":
            if len(args)<2:raise ValueError("Invalid extended import")
            raw=struct.pack("<"+"I"*(len(args)-1),*args[1:]);end=raw.find(b"\0")
            if end<0:raise ValueError("Unterminated import")
            imports[args[0]]=raw[:end]
            if imports[args[0]]!=b"GLSL.std.450":reject("unsupported_extended_import")
        elif name=="OpEntryPoint":
            if len(args)<3:raise ValueError("Invalid entry point")
            entry_models.append(args[0]);entry_functions.append(args[1])
        elif name=="OpMemoryModel":models.append(tuple(args))
        elif name=="OpFunction":
            if len(args)<4:raise ValueError("Invalid function")
            current=args[1];functions.add(current)
        elif name=="OpLabel" and current is not None:bodies.add(current)
        elif name=="OpFunctionEnd":current=None
        elif name=="OpFunctionCall":
            if len(args)<3:raise ValueError("Invalid call")
            calls.append(args[2])
    if len(entry_models)!=1 or entry_models[0] not in (0,4):reject("not_single_vertex_or_fragment_entry")
    if len(models)!=1 or models[0] not in ((0,1),(5348,1),(0,3),(5348,3)):reject("unsupported_memory_model")
    if any(f not in bodies for f in entry_functions+calls):reject("unresolved_function_body")
    def pointer_class(result):return pointer_types.get(result_types.get(result))
    for opcode,args in ops:
        info=definitions.get(opcode)
        if info is None:continue
        name,kind=info["opname"],info.get("class","")
        if name in ("OpStore","OpCopyMemory","OpCopyMemorySized"):
            if not args:raise ValueError("Missing write pointer")
            storage=pointer_class(args[0]);writes[str(storage)]+=1
            if storage not in LOCAL_WRITES:reject("nonlocal_or_unresolved_write")
        if name in ("OpCopyMemory","OpCopyMemorySized"):
            if len(args)<2:raise ValueError("Missing copy source")
            storage=pointer_class(args[1]);reads[str(storage)]+=1
            if storage not in READ_CLASSES:reject("hidden_or_unresolved_read")
        if name in ("OpLoad","OpArrayLength"):
            if len(args)<3:raise ValueError("Missing load pointer")
            storage=pointer_class(args[2]);reads[str(storage)]+=1
            if storage not in READ_CLASSES:reject("hidden_or_unresolved_read")
        if name=="OpVariable" and len(args)>=3 and pointer_class(args[1])!=args[2]:reject("incoherent_variable_storage")
        if name in ("OpConvertUToPtr","OpGenericCastToPtr","OpGenericCastToPtrExplicit","OpPtrCastToGeneric"):
            reject("unsupported_pointer_conversion")
        if name=="OpBitcast" and len(args)>=2 and args[0] in pointer_types:reject("unsupported_pointer_bitcast")
        if name in ("OpDecorate","OpDecorateId","OpMemberDecorate"):
            at=2 if name=="OpMemberDecorate" else 1
            if len(args)<=at:raise ValueError("Invalid decoration")
            if args[at] in (36,37,41):reject("transform_feedback_or_linkage")
        if name in ("OpExecutionMode","OpExecutionModeId") and len(args)>=2 and args[1]==11:reject("transform_feedback_mode")
        allowed=False
        if opcode<400 and kind in PURE_CLASSES:allowed=True
        elif opcode<400 and kind=="Image" and name!="OpImageWrite":allowed=True
        elif name in MEMORY|CONTROL|FUNCTIONS:allowed=True
        elif name in {"OpNop","OpUndef","OpExtension","OpExtInstImport","OpMemoryModel","OpEntryPoint",
                      "OpExecutionMode","OpExecutionModeId","OpCapability","OpDecorate","OpMemberDecorate",
                      "OpDecorateId","OpDecorationGroup","OpGroupDecorate","OpGroupMemberDecorate"}:allowed=True
        elif opcode<400 and kind=="Conversion":allowed=True # Pointer conversions/bitcasts rejected separately.
        elif name=="OpExtInst":
            if len(args)<4:raise ValueError("Invalid extended instruction")
            allowed=imports.get(args[2])==b"GLSL.std.450"
            # Some GLSL extended instructions write through out-pointer operands.
            # Accept only value operands here, even when a local pointer could be safe.
            if any(pointer_class(value) is not None for value in args[4:]):reject("pointer_extended_operand")
            if args[0] in pointer_types:reject("pointer_extended_result")
        if not allowed:reject("unsupported_operation:"+name)
    return {"schema":1,"instruction_count":len(ops),"entry_models":entry_models,
            "read_storage_classes":dict(reads),"write_storage_classes":dict(writes),
            "rejections":dict(sorted(reasons.items())),"supported_read_only_subset":not reasons,
            "spirv_validation_performed":False,"symbolic_range_coverage_complete":False,
            "safe_to_patch":False,"safe_to_replay":False,
            "limitations":["Assumes valid SPIR-V; this is not a validator.",
                "Checks shader stores, not query/transform-feedback/pipeline effects or descriptor identity.",
                "Does not prove camera semantics, culling, temporal correctness or complete scene coverage."]}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module",type=Path);parser.add_argument("--grammar",type=Path,required=True)
    parser.add_argument("--output",type=Path)
    args=parser.parse_args()
    if args.module.stat().st_size>8*1024*1024 or args.grammar.stat().st_size>8*1024*1024:parser.error("Input exceeds bounds")
    data=args.module.read_bytes()
    if len(data)%4:parser.error("Module is not word aligned")
    try:report=analyze(struct.unpack("<"+"I"*(len(data)//4),data),json.loads(args.grammar.read_text(encoding="utf-8")))
    except (ValueError,KeyError,TypeError) as error:parser.error(str(error))
    report["module_sha256"]=hashlib.sha256(data).hexdigest()
    text=json.dumps(report,indent=2)+"\n"
    if args.output:args.output.write_text(text,encoding="utf-8")
    else:print(text,end="")
if __name__=="__main__":main()
