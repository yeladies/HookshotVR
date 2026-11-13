# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a PRIVATE frozen depth mesh for XR calibration; never a live stereo renderer."""
import argparse,json,math,struct
from pathlib import Path

def unpack_channel(bits,mantissa):
    fraction=bits&((1<<mantissa)-1);exponent=bits>>mantissa
    if exponent==31:return 0.0
    return math.ldexp(fraction,-14-mantissa) if exponent==0 else math.ldexp(1+fraction/(1<<mantissa),exponent-15)

def reconstruct(x,y,depth,width,height,p):
    if not math.isfinite(depth) or not 0<=depth<0.999999:return None
    denominator=2*depth-1+p[10]
    if abs(denominator)<1e-12:return None
    z=-p[11]/denominator
    if not math.isfinite(z) or z>=0:return None
    return (-z*((2*(x+.5)/width-1)+p[2])/p[0],-z*((1-2*(y+.5)/height)+p[6])/p[5],z)

def convert(source,destination,scale=1.,step=6,max_distance=80):
    if not math.isfinite(scale) or not .0001<=scale<=10 or not 1<=step<=32:raise ValueError('Invalid scale or sampling step')
    if Path(source).stat().st_size>16*1024*1024+132:raise ValueError("Capture exceeds byte budget")
    data=Path(source).read_bytes()
    if len(data)<132 or data[:8]!=b'HSVRD2\0\0':raise ValueError('Expected a version-two capture with explicit color view format')
    width,height,view_format=struct.unpack_from('<III',data,8);p=struct.unpack_from('<16f',data,20);v=struct.unpack_from('<12f',data,84)
    if view_format not in (122,123):raise ValueError('Unsupported captured color view')
    if not 1<=width<=1920 or not 1<=height<=1080 or len(data)!=132+width*height*8:raise ValueError('Capture size/budget mismatch')
    if not all(math.isfinite(x) for x in (*p,*v)) or p[0]<=0 or p[5]<=0 or abs(p[14]+1)>1e-5 or abs(p[15])>1e-5:raise ValueError('Unsupported source projection')
    if any(abs(sum(v[4*r+k]*v[4*s+k] for k in range(3))-(r==s))>.003 for r in range(3) for s in range(3)):raise ValueError('Source view is not rigid')
    yaw=math.atan2(v[8],v[10]);co,si=math.cos(yaw),math.sin(yaw)
    xs=list(range(0,width,step));ys=list(range(0,height,step))
    if xs[-1]!=width-1:xs.append(width-1)
    if ys[-1]!=height-1:ys.append(height-1)
    if (len(xs)-1)*(len(ys)-1)*6>1000000:raise ValueError("Sampling exceeds vertex budget; increase step")
    points=[];depths=[]
    for y in ys:
        row=[]
        for x in xs:
            index=y*width+x;depth=struct.unpack_from('<f',data,132+width*height*4+index*4)[0];point=reconstruct(x,y,depth,width,height,p)
            if point is None:row.append(None);continue
            depths.append(-point[2]);world=[sum(v[4*r+k]*point[r] for r in range(3)) for k in range(3)]
            position=((co*world[0]-si*world[2])*scale,world[1]*scale,(si*world[0]+co*world[2])*scale)
            distance=math.sqrt(sum(c*c for c in position))
            if distance>max_distance:row.append(None);continue
            pixel=struct.unpack_from('<I',data,132+index*4)[0]
            if view_format==123:
                multiplier=math.ldexp(1.,(pixel>>27)-24);rgb=tuple(((pixel>>shift)&511)*multiplier for shift in (0,9,18))
            else:rgb=(unpack_channel(pixel&2047,6),unpack_channel((pixel>>11)&2047,6),unpack_channel(pixel>>22,5))
            # Gameplay upscale input is square-root encoded LDR (vk_temporal.h).
            color=tuple(min(1.,max(0.,c))**2 for c in rgb)
            row.append((*position,*color,distance))
        points.append(row)
    vertices=[];rejected=0
    for y in range(len(ys)-1):
        for x in range(len(xs)-1):
            a,b,c,d=points[y][x],points[y][x+1],points[y+1][x],points[y+1][x+1]
            for triangle in ((a,c,b),(b,c,d)):
                if any(q is None for q in triangle):rejected+=1;continue
                distances=[q[6] for q in triangle]
                if max(distances)>min(distances)*1.15:rejected+=1;continue
                for q in triangle:vertices.extend(q[:6])
    count=len(vertices)//6
    if not 3<=count<=1000000:raise ValueError('No bounded visible mesh; inspect capture and scale')
    payload=struct.pack('<8sIf',b'HSVRM1\0\0',count,scale)+struct.pack('<'+'f'*len(vertices),*vertices)
    Path(destination).write_bytes(payload)
    depths.sort()
    report={'vertices':count,'triangles':count//3,'rejected_triangles':rejected,'color_view_format':view_format,'source_width':width,'source_height':height,'metres_per_game_unit':scale,'source_depth_percentiles':{str(q):depths[min(len(depths)-1,int((len(depths)-1)*q))] for q in (.1,.5,.9)},'mesh_bytes':len(payload),'frozen_scene':True,'native_stereo_render':False,'game_derived_private_output':True}
    Path(str(destination)+'.json').write_text(json.dumps(report,indent=2)+'\n');return report

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('capture',type=Path);parser.add_argument('mesh',type=Path);parser.add_argument('--scale',type=float,default=1.);parser.add_argument('--step',type=int,default=6)
    args=parser.parse_args()
    try:print(json.dumps(convert(args.capture,args.mesh,args.scale,args.step),indent=2))
    except (ValueError,OSError,struct.error) as error:parser.error(str(error))
if __name__=='__main__':main()
