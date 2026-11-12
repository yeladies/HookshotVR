"""Authored SPIR-V instruction fragments; no game modules or driver required."""
import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('audit_spv',Path(__file__).resolve().parents[1]/'audit_spirv_uniforms.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)

def op(code,*args): return [(len(args)+1)<<16|code,*args]
def module(dynamic=False,descriptor_array=False):
    words=[0x07230203,0x00010500,0,100,0]
    for instruction in [op(21,1,32,0),op(23,2,1,4),op(43,1,3,0),op(43,1,4,7),op(43,1,5,64),
                        op(28,6,2,5),op(71,6,6,16),op(30,7,6),op(71,7,2),op(72,7,0,35,0)]: words+=instruction
    if descriptor_array:
        words+=op(28,8,7,5);words+=op(32,9,2,8)
    else: words+=op(32,9,2,7)
    words+=op(32,10,2,2)+op(59,9,11,2)+op(71,11,33,6)+op(71,11,34,0)
    indices=[3,3,99 if dynamic else 4] if descriptor_array else [3,99 if dynamic else 4]
    words+=op(65,10,12,11,*indices)+op(61,2,13,12)
    return words

class SpirvUniformTests(unittest.TestCase):
    def test_static_vector_read_maps_to_projection_row(self):
        result=audit.analyze(module(),6)
        self.assertEqual(result['static_read_word_offsets'],[112,116,120,124])
        self.assertEqual(result['camera_regions_read'],['projection'])
        self.assertTrue(result['static_subset_resolved']);self.assertFalse(result['safe_to_patch'])
    def test_other_binding_is_not_a_camera_read(self):
        result=audit.analyze(module(),7)
        self.assertEqual(result['matching_uniform_variables'],0)
        self.assertEqual(result['static_loads'],0)
    def test_dynamic_index_does_not_invent_an_offset(self):
        result=audit.analyze(module(dynamic=True),6)
        self.assertEqual(result['dynamic_or_unsupported_target_loads'],1)
        self.assertFalse(result['static_subset_resolved'])
    def test_descriptor_array_element_is_separate_from_byte_offset(self):
        result=audit.analyze(module(descriptor_array=True),6,array_element=0)
        self.assertEqual(result['static_read_word_offsets'],[112,116,120,124])
        other=audit.analyze(module(descriptor_array=True),6,array_element=1)
        self.assertEqual(other['static_loads'],0)
    def test_truncated_and_out_of_range_inputs(self):
        with self.assertRaises(ValueError): audit.analyze(module()[:-1],6)
        result=audit.analyze(module(),6,byte_limit=100)
        self.assertEqual(result['out_of_range_target_loads'],1)
        self.assertFalse(result['static_subset_resolved'])
    def test_storage_buffer_read_is_not_an_unknown_uniform(self):
        words=module()+op(32,20,12,7)+op(32,24,12,2)+op(59,20,21,12)+op(65,24,22,21,3,4)+op(61,2,23,22)
        result=audit.analyze(words,6)
        self.assertEqual(result['static_loads'],1)
        self.assertEqual(result['unknown_uniform_pointer_loads'],0)
        self.assertTrue(result['static_subset_resolved'])
    def test_unknown_pointer_copy_stays_unresolved(self):
        words=module()[:-4]+op(55,10,14)+op(61,2,15,14)
        result=audit.analyze(words,6)
        self.assertEqual(result['unknown_uniform_pointer_loads'],1)
        self.assertFalse(result['static_subset_resolved'])

if __name__=='__main__':unittest.main()
