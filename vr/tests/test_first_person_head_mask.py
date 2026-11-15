"""Head masking fixture: six invented vertices and four invented bones."""
import importlib.util
import struct
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('head_mask',Path(__file__).parents[1]/'first_person_head_mask.py')
mask=importlib.util.module_from_spec(spec);spec.loader.exec_module(mask)


def fixture():
    b=bytearray(8192);b[:4]=b'FRES';b[8:12]=bytes((0,4,10,0));b[12:14]=b'\xff\xfe'
    def put(fmt,at,*values):struct.pack_into('<'+fmt,b,at,*values)
    def string(at,text):put('H',at,len(text));b[at+2:at+2+len(text)]=text.encode()
    b[256:260]=b'FSKL';put('QQ',272,320,704);put('HHH',312,4,4,0);put('HHHH',704,0,1,2,3)
    for k,name in enumerate(('Root','Head','Eyeball_L','Eyeball_R')):
        at=320+88*k;name_at=2048+32*k;string(name_at,name);put('Q',at,name_at);put('Hh',at+32,k,(-1,0,1,1)[k]);put('I',at+44,0x1000)
        put('3f',at+48,1,1,1);put('4f',at+60,0,0,0,1);put('3f',at+76,*((0,0,0),(0,1,0),(-.03,.05,.08),(.03,.05,.08))[k])
    put('Q',176,768);put('Q',776,4096)
    b[896:900]=b'FSHP';string(2176,'InventedMixedHeadAndHands');put('QQQ',904,2176,1024,1152);b[986]=2;b[987]=1
    b[1024:1028]=b'FVTX';put('Q',1032,1328);put('QQ',1072,1280,1296);put('I',1096,64);b[1100]=2;b[1101]=1;put('I',1104,6);put('H',1110,8)
    put('Q',1176,1216);put('IIIIIH',1184,0,3,1,6,0,1);put('I',1216,12);put('I',1280,24);put('I',1296,4)
    for k,(name,fmt,offset) in enumerate((('_i0',0x309,0),('_w0',0x109,2))):
        at=1328+16*k;np=2240+32*k;string(np,name);put('Q',at,np);struct.pack_into('>H',b,at+8,fmt);put('HH',at+12,offset,0)
    put('HHHHHH',4096,0,1,2,3,4,5)
    for k in range(6):b[4160+4*k:4164+4*k]=bytes((1 if k<3 else 0,0 if k<3 else 1,255,0))
    return b


class Mask(unittest.TestCase):
    def test_selective_triangle_mask(self):
        source=fixture();out,report=mask.hide_head(source)
        self.assertEqual(struct.unpack_from('<6H',out,4096),(0,0,0,3,4,5))
        self.assertEqual(out[:4096],source[:4096]);self.assertEqual(out[4108:],source[4108:])
        self.assertEqual(len(out),len(source));self.assertTrue(report['same_uncompressed_size'])
        self.assertEqual(report['shapes'][0]['removed'],1);self.assertEqual(report['shapes'][0]['triangles'],2)
    def test_second_lod(self):
        source=fixture();source[987]=2;source[1208:1264]=source[1152:1208]
        # Relocate shared size metadata away from the second mesh record.
        struct.pack_into('<Q',source,1176,1272);struct.pack_into('<Q',source,1232,1272);struct.pack_into('<I',source,1272,12)
        struct.pack_into('<I',source,1240,16);source[4112:4124]=source[4096:4108]
        out,report=mask.hide_head(source)
        self.assertEqual(struct.unpack_from('<6H',out,4112),(0,0,0,3,4,5));self.assertEqual(report['shapes'][0]['removed'],2)
    def test_bad_inputs_fail_before_output(self):
        for at,fmt,values in ((238,'B',(8,)),(1188,'I',(4,)),(1192,'I',(0,)),(1196,'I',(7,)),
            (1096,'I',(999999,)),(4096,'H',(100,)),(4160,'B',(99,)),(4162,'B',(0,))):
            with self.subTest(at=at):
                source=fixture();struct.pack_into('<'+fmt,source,at,*values);before=bytes(source)
                with self.assertRaises(ValueError):mask.hide_head(source)
                self.assertEqual(bytes(source),before)
    def test_threshold(self):
        for value in (0,-1,2,float('nan')):
            with self.assertRaises(ValueError):mask.hide_head(fixture(),value)


if __name__=='__main__':unittest.main()
