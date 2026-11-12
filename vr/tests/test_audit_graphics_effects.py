"""Authored instruction fragments, not game modules or complete SPIR-V validation."""
import importlib.util
from pathlib import Path
import struct
import unittest
spec=importlib.util.spec_from_file_location("graphics_effects",Path(__file__).resolve().parents[1]/"audit_graphics_effects.py")
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)

def op(code,*args):return [(len(args)+1)<<16|code,*args]
def string_words(text):
    data=text.encode()+b"\0";data+=b"\0"*((-len(data))%4)
    return struct.unpack("<"+"I"*(len(data)//4),data)
def info(code,name,kind,*operands):return {"opcode":code,"opname":name,"class":kind,"operands":[{"kind":x} for x in operands]}
GRAMMAR={"instructions":[
    info(14,"OpMemoryModel","Mode-Setting","AddressingModel","MemoryModel"),
    info(15,"OpEntryPoint","Mode-Setting","ExecutionModel","IdRef","LiteralString"),
    info(16,"OpExecutionMode","Mode-Setting","IdRef","ExecutionMode"),
    info(17,"OpCapability","Mode-Setting","Capability"),
    info(19,"OpTypeVoid","Type-Declaration","IdResult"),info(22,"OpTypeFloat","Type-Declaration","IdResult","LiteralInteger"),
    info(32,"OpTypePointer","Type-Declaration","IdResult","StorageClass","IdRef"),
    info(33,"OpTypeFunction","Type-Declaration","IdResult","IdRef"),
    info(43,"OpConstant","Constant-Creation","IdResultType","IdResult","LiteralContextDependentNumber"),
    info(54,"OpFunction","Function","IdResultType","IdResult","FunctionControl","IdRef"),
    info(56,"OpFunctionEnd","Function"),info(57,"OpFunctionCall","Function","IdResultType","IdResult","IdRef"),
    info(59,"OpVariable","Memory","IdResultType","IdResult","StorageClass"),
    info(61,"OpLoad","Memory","IdResultType","IdResult","IdRef"),info(62,"OpStore","Memory","IdRef","IdRef"),
    info(63,"OpCopyMemory","Memory","IdRef","IdRef"),info(71,"OpDecorate","Annotation","IdRef","Decoration"),
    info(99,"OpImageWrite","Image","IdRef","IdRef","IdRef"),
    info(120,"OpConvertUToPtr","Conversion","IdResultType","IdResult","IdRef"),
    info(124,"OpBitcast","Conversion","IdResultType","IdResult","IdRef"),
    info(234,"OpAtomicIAdd","Atomic","IdResultType","IdResult","IdRef"),
    info(248,"OpLabel","Control-Flow","IdResult"),info(253,"OpReturn","Control-Flow"),
    info(11,"OpExtInstImport","Extension","IdResult","LiteralString"),
    info(12,"OpExtInst","Extension","IdResultType","IdResult","IdRef","LiteralExtInstInteger"),
]}
def module(extra=(),model=0,memory=(0,1)):
    words=[0x07230203,0x00010500,0,1000,0]
    for item in (op(17,1),op(14,*memory),op(15,model,100,*string_words("main")),
        op(19,1),op(22,2,32),op(32,3,7,2),op(32,4,3,2),op(32,5,12,2),op(32,6,5349,2),op(33,7,1),
        op(43,2,8,0x3f800000),op(59,4,10,3),op(59,5,11,12),op(54,1,100,0,7),op(248,101),
        op(59,3,20,7),op(61,2,21,11),op(62,20,21),op(62,10,8)):
        words+=item
    for item in extra:words+=item
    return words+op(253)+op(56)
class EffectsTests(unittest.TestCase):
    def result(self,extra=(),**kwargs):return audit.analyze(module(extra,**kwargs),GRAMMAR)
    def test_local_and_output_stores_are_only_candidates(self):
        result=self.result();self.assertTrue(result["supported_read_only_subset"])
        for key in ("safe_to_replay","safe_to_patch","symbolic_range_coverage_complete","spirv_validation_performed"):self.assertFalse(result[key])
    def test_storage_buffer_store_rejected(self):self.assertFalse(self.result([op(62,11,8)])["supported_read_only_subset"])
    def test_unresolved_store_rejected(self):self.assertIn("nonlocal_or_unresolved_write",self.result([op(62,999,8)])["rejections"])
    def test_physical_read_rejected(self):
        result=self.result([op(59,6,12,5349),op(61,2,30,12)],memory=(5348,1))
        self.assertIn("hidden_or_unresolved_read",result["rejections"])
    def test_copy_checks_both_pointer_scopes(self):
        self.assertIn("hidden_or_unresolved_read",self.result([op(59,6,12,5349),op(63,20,12)])["rejections"])
        self.assertIn("nonlocal_or_unresolved_write",self.result([op(63,11,20)])["rejections"])
    def test_atomic_rejected(self):self.assertFalse(self.result([op(234,2,30,11,8,8,8)])["supported_read_only_subset"])
    def test_image_write_rejected(self):self.assertFalse(self.result([op(99,11,8,8)])["supported_read_only_subset"])
    def test_pointer_conversion_and_bitcast_rejected(self):
        self.assertFalse(self.result([op(120,6,30,8)])["supported_read_only_subset"])
        self.assertFalse(self.result([op(124,3,30,20)])["supported_read_only_subset"])
    def test_unknown_extended_import_rejected(self):
        self.assertFalse(self.result([op(11,60,*string_words("NonSemantic.DebugPrintf")),op(12,2,30,60,1,8)])["supported_read_only_subset"])
    def test_known_math_extended_import(self):
        self.assertTrue(self.result([op(11,60,*string_words("GLSL.std.450")),op(12,2,30,60,1,8)])["supported_read_only_subset"])
    def test_extended_out_pointers_are_not_assumed_pure_math(self):
        for pointer in (11,20):
            result=self.result([op(11,60,*string_words("GLSL.std.450")),op(12,2,30,60,35,8,pointer)])
            self.assertIn("pointer_extended_operand",result["rejections"])
    def test_transform_feedback_is_not_ordinary_output(self):
        self.assertFalse(self.result([op(71,10,36,0)])["supported_read_only_subset"])
        self.assertFalse(self.result([op(16,100,11)])["supported_read_only_subset"])
    def test_external_call_rejected(self):self.assertIn("unresolved_function_body",self.result([op(57,1,30,999)])["rejections"])
    def test_compute_and_unknown_model_rejected(self):
        for model in (5,5267):self.assertFalse(self.result(model=model)["supported_read_only_subset"])
    def test_unknown_operation_rejected(self):self.assertIn("unknown_opcode",self.result([op(65500)])["rejections"])
    def test_duplicate_ids_and_malformed_instructions_fail(self):
        with self.assertRaises(ValueError):self.result([op(59,3,20,7)])
        with self.assertRaises(ValueError):audit.analyze(module()+[0],GRAMMAR)
    def test_physical_model_alone_does_not_invent_reads(self):self.assertTrue(self.result(memory=(5348,1))["supported_read_only_subset"])
    def test_input_is_unchanged(self):
        words=module();before=words.copy();audit.analyze(words,GRAMMAR);self.assertEqual(words,before)
if __name__=="__main__":unittest.main()
