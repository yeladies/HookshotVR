# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util,struct,tempfile,unittest,math,hashlib,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('depth_mesh',root/'depth_scene_mesh.py');mesh=importlib.util.module_from_spec(spec);spec.loader.exec_module(mesh)
class DepthTests(unittest.TestCase):
 def setUp(self):self.tmp=tempfile.TemporaryDirectory();self.path=Path(self.tmp.name)
 def tearDown(self):self.tmp.cleanup()
 def source(self,w=8,h=8,fmt=123):
  p=[0.]*16;p[0]=p[5]=1;p[10]=-(100+.1)/(100-.1);p[11]=-2*100*.1/(100-.1);p[14]=-1
  v=[1,0,0,0,0,1,0,0,0,0,1,0];z=-5;depth=(-p[10]-p[11]/z+1)/2
  color=(16<<27)|(256<<18)|(256<<9)|256 if fmt==123 else (15<<6)|((15<<6)<<11)|((15<<5)<<22)
  data=struct.pack('<8sIII16f12f',b'HSVRD2\0\0',w,h,fmt,*p,*v)+struct.pack('<I',color)*(w*h)+struct.pack('<f',depth)*(w*h)
  path=self.path/'source.hsvrd';path.write_bytes(data);return path,p,depth
 def test_plane_reconstructs_negative_z(self):
  _,p,d=self.source();point=mesh.reconstruct(3.5,3.5,d,8,8,p)
  self.assertAlmostEqual(point[0],0);self.assertAlmostEqual(point[1],0);self.assertAlmostEqual(point[2],-5)
 def test_two_color_formats_and_complete_mesh(self):
  for fmt in (122,123):
   source,_,_=self.source(fmt=fmt);output=self.path/'mesh.hsvrm';report=mesh.convert(source,output,1,2)
   data=output.read_bytes();self.assertEqual(data[:8],b'HSVRM1\0\0');self.assertEqual(len(data),16+24*report['vertices']);self.assertEqual(report['color_view_format'],fmt)
   for v in struct.iter_unpack('<6f',data[16:]):self.assertAlmostEqual(v[2],-5,places=4);self.assertEqual(v[3:],(1.,1.,1.))
 def test_invalid_depth_never_becomes_geometry(self):
  _,p,_=self.source()
  for d in (math.nan,math.inf,-1,1,2):self.assertIsNone(mesh.reconstruct(0,0,d,8,8,p))
 def test_headers_size_and_color_format_reject(self):
  source,_,_=self.source();original=source.read_bytes()
  for data in (b'',original[:-4],b'HSVRD1\0\0'+original[8:],original[:16]+struct.pack('<I',999)+original[20:]):
   source.write_bytes(data)
   with self.assertRaises(ValueError):mesh.convert(source,self.path/'output.hsvrm')
 def test_bounded_sampling_scale(self):
  source,_,_=self.source()
  for scale,step in ((0,6),(math.nan,6),(100,6),(1,0),(1,33)):
   with self.assertRaises(ValueError):mesh.convert(source,self.path/'mesh',scale,step)
  source,_,_=self.source(800,600)
  with self.assertRaises(ValueError):mesh.convert(source,self.path/'mesh',1,1)
 def test_authored_shader_header_tracks_sources(self):
  text=(root/'src/depth_scene_spv.h').read_text(encoding='utf-8');hashes=re.findall('source-sha256: ([0-9a-f]{64})',text)
  self.assertEqual(hashes,[hashlib.sha256((root/'shaders'/('depth_scene.'+stage)).read_bytes()).hexdigest() for stage in ('vert','frag')])
  self.assertEqual(text.count('0x7230203'),2)
if __name__=='__main__':unittest.main()
