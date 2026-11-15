"""Authored synthetic bones only: no game assets required."""
import importlib.util
import math
import struct
import unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('first_person_assets',Path(__file__).parents[1]/'first_person_assets.py')
assets=importlib.util.module_from_spec(spec);spec.loader.exec_module(assets)


def fixture():
    b=bytearray(2048);b[:4]=b'FRES';b[8:12]=bytes((0,4,10,0));b[12:14]=b'\xff\xfe';b[64:68]=b'FSKL'
    struct.pack_into('<Q',b,80,128);struct.pack_into('<H',b,120,4)
    names=('Root','Head','Eyeball_L','Eyeball_R')
    for k,name in enumerate(names):
        at=128+88*k;offset=1024+32*k
        struct.pack_into('<Q',b,at,offset);struct.pack_into('<H',b,offset,len(name));b[offset+2:offset+2+len(name)]=name.encode()
        struct.pack_into('<Hh',b,at+32,k,(-1,0,1,1)[k]);struct.pack_into('<I',b,at+44,0x1000)
        struct.pack_into('<3f',b,at+48,1,1,1);struct.pack_into('<4f',b,at+60,0,0,0,1)
        position=((0,0,0),(0,1,0),(-.03,.05,.08),(.03,.05,.08))[k]
        struct.pack_into('<3f',b,at+76,*position)
    return b


class Bones(unittest.TestCase):
    def test_bind_midpoint(self):
        p=assets.bind_eye_profile(fixture());self.assertAlmostEqual(p['eye_height'],1.05,places=6)
        self.assertAlmostEqual(p['forward_offset'],.08,places=6);self.assertAlmostEqual(p['lateral_offset'],0)
        self.assertEqual(p['bone_count'],4)
    def test_parent_rotation(self):
        b=fixture();struct.pack_into('<4f',b,128+88+60,0,math.pi/2,0,1)
        p=assets.bind_eye_profile(b);self.assertAlmostEqual(p['forward_offset'],0,places=6)
        self.assertAlmostEqual(p['lateral_offset'],.08,places=6)
    def test_quaternion(self):
        b=fixture();struct.pack_into('<I',b,128+88+44,0)
        struct.pack_into('<4f',b,128+88+60,0,math.sin(math.pi/4),0,math.cos(math.pi/4))
        self.assertAlmostEqual(assets.bind_eye_profile(b)['lateral_offset'],.08,places=6)
    def test_malformed(self):
        for offset,fmt,values in ((8,'<I',(0,)),(80,'<Q',(999999,)),(120,'<H',(9999,)),
             (128+32,'<Hh',(0,0)),(128+88+34,'<h',(40,)),(128+76,'<f',(float('nan'),))):
            with self.subTest(offset=offset):
                b=fixture();struct.pack_into(fmt,b,offset,*values)
                with self.assertRaises(ValueError):assets.bind_eye_profile(b)
    def test_missing_eye_and_truncated(self):
        b=fixture();b[1024+64+2]=ord('X')
        with self.assertRaises(ValueError):assets.bind_eye_profile(b)
        with self.assertRaises(ValueError):assets.bind_eye_profile(fixture()[:400])


if __name__=='__main__':unittest.main()
