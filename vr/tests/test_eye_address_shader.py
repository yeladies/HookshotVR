"""Check authored generated shader freshness and its bounded execution contract."""
import hashlib
from pathlib import Path
import re
import unittest
class ShaderTests(unittest.TestCase):
    def test_generated_source_and_compute_contract(self):
        root=Path(__file__).resolve().parents[1]
        source=(root/"shaders/eye_address_copy.comp").read_bytes().replace(b"\r\n",b"\n")
        header=(root/"src/eye_address_copy_spv.h").read_text()
        self.assertIn("Source SHA256: "+hashlib.sha256(source).hexdigest(),header,"Regenerate after editing the authored shader")
        words=[int(w,16) for w in re.findall(r"0x[0-9a-f]{8}",header)]
        self.assertGreater(len(words),5);self.assertLessEqual(len(words)*4,65536)
        self.assertEqual(words[0],0x07230203)
        instructions=[];offset=5
        while offset<len(words):
            count=words[offset]>>16
            self.assertGreater(count,0);self.assertLessEqual(offset+count,len(words))
            instructions.append((words[offset]&65535,words[offset+1:offset+count]));offset+=count
        self.assertEqual(offset,len(words))
        self.assertIn((17,[5347]),instructions) # PhysicalStorageBufferAddresses
        self.assertIn((14,[5348,1]),instructions) # PhysicalStorageBuffer64 / GLSL450
        entries=[args for op,args in instructions if op==15]
        self.assertEqual(len(entries),1);self.assertEqual(entries[0][0],5) # GLCompute
        self.assertIn((16,[entries[0][1],17,64,1,1]),instructions) # LocalSize
if __name__=="__main__":unittest.main()
