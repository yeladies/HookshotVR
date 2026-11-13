# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual reviewed perspective hook against authored registers; no game."""
from pathlib import Path
import argparse,subprocess,sys,tempfile,os
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'nx2'))
import oot_wide
parser=argparse.ArgumentParser();parser.add_argument('--cxx',required=True);args=parser.parse_args()
fixture=r"""
#include <cstdint>
#include <cstring>
#include <cmath>
#include <stdexcept>
struct Ctx {uint64_t x[32]{};unsigned char vreg[32][16]{};};
volatile float hos_wide_ratio=0,hos_vr_fov=0,hos_vr_aspect=0;
void a64_vset64(unsigned char* out,uint64_t value){std::memcpy(out,&value,8);}
void recomp_store32(Ctx*,uint64_t at,uint32_t value){std::memcpy(reinterpret_cast<void*>(uintptr_t(at)),&value,4);}
void patch(Ctx* c){SITE}
float lane(Ctx& c,int reg){float out;std::memcpy(&out,c.vreg[reg],4);return out;}
void input(Ctx& c,unsigned char* object,float fov,float ratio){c.x[BASE]=uintptr_t(object);std::memcpy(c.vreg[0],&fov,4);std::memcpy(c.vreg[RATIO],&ratio,4);}
void check(bool b){if(!b)throw std::runtime_error("lens hook register/culling mismatch");}
int main(){Ctx c;alignas(16) unsigned char object[512]{};input(c,object,.7f,16.f/9);patch(&c);check(lane(c,0)==.7f&&lane(c,RATIO)==16.f/9);
hos_vr_fov=1.1917536f;hos_vr_aspect=.9f;patch(&c);check(lane(c,0)==1.1917536f&&lane(c,RATIO)==.9f);float f,a;std::memcpy(&f,object+0xac,4);std::memcpy(&a,object+0xb0,4);check(f==1.1917536f&&a==.9f);
input(c,object,.7f,1.f);patch(&c);check(lane(c,0)==.7f&&lane(c,RATIO)==1.f);
input(c,object,.7f,16.f/9);hos_vr_fov=3.2f;patch(&c);check(lane(c,0)==.7f);
hos_vr_fov=0;hos_vr_aspect=0;hos_wide_ratio=2.3f;patch(&c);check(lane(c,RATIO)==2.3f&&lane(c,0)==.7f);return 0;}
"""
with tempfile.TemporaryDirectory(prefix='hookshot-lens-') as temp:
    for reg,base in ((1,0),(3,21),(3,0),(1,2)):
        source=Path(temp)/f'fixture-{reg}-{base}.cpp';exe=source.with_suffix('.exe')
        source.write_text(fixture.replace('SITE',oot_wide._ratio(reg,base)).replace('BASE',str(base)).replace('RATIO',str(reg)),encoding='utf-8')
        subprocess.run([args.cxx,'-std=c++17','-Wall','-Wextra','-Werror',*(['-static'] if os.name=='nt' else []),str(source),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=10)
print('Perspective hook updates both matrix inputs and CPU frustum; default/other projections preserved.')
