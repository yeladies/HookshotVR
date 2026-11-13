# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit GPU test with a synthetic moving-depth plane; never opens XR or reads a game."""
import argparse,json,math,os,struct,subprocess,tempfile,threading,time
from pathlib import Path

def run(viewer):
 with tempfile.TemporaryDirectory(prefix='vr-depth-motion-') as temporary:
  root=Path(temporary);frame=root/'frame.hsvrd';stop=threading.Event();errors=[]
  w,h=128,96;p=[0.]*16;p[0]=p[5]=1;p[10]=-(100+.1)/(100-.1);p[11]=-2*100*.1/(100-.1);p[14]=-1
  view=[1,0,0,0,0,1,0,0,0,0,1,0];color=struct.pack('<'+'I'*(w*h),*[(16<<27)|256 if w//4<=x<3*w//4 and h//4<=y<3*h//4 else 0 for y in range(h) for x in range(w)])
  def publish(index):
   distance=1.5+math.sin(index*.25)*.8;z=-distance;depth=(-p[10]-p[11]/z+1)/2
   payload=struct.pack('<8sIII16f12f',b'HSVRD2\0\0',w,h,123,*p,*view)+color+struct.pack('<f',depth)*(w*h)
   target=root/'next.tmp';target.write_bytes(payload)
   try:os.replace(target,frame)
   except PermissionError:pass  # Windows readers may briefly prevent replacement; keep the last complete frame.
  publish(0)
  def producer():
   try:
    for index in range(1,200):
     if stop.wait(.1):break
     publish(index)
   except Exception as error:errors.append(str(error))
  worker=threading.Thread(target=producer);worker.start()
  try:
   result=subprocess.run([str(Path(viewer).resolve()),'--stream-check',str(frame),'--output',str(root/'eyes'),'--seconds','5'],capture_output=True,text=True,timeout=25)
  finally:stop.set();worker.join()
  if errors:raise RuntimeError(errors)
  if result.returncode:raise RuntimeError(result.stderr+'\n'+result.stdout)
  report=json.loads(result.stdout)
  if report['source_color_changed_frames']!=0 or report['source_depth_changed_frames']<2 or report['temporal_different_pixels']<100:raise RuntimeError('Depth-only motion was not reproduced: '+str(report))
  return report
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--viewer',type=Path,required=True);args=parser.parse_args();print(json.dumps(run(args.viewer),indent=2))
