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
#include "VIEW_HEADER"
struct Ctx {uint64_t x[32]{};unsigned char vreg[32][16]{};};
volatile float hos_wide_ratio=0,hos_vr_fov=0,hos_vr_aspect=0,hos_vr_cull_tangent=0;
unsigned observations=0;float observed_raster=0,observed_visibility=0;
void hos_vr_visibility_observed(uint64_t,float raster,float visibility,float){++observations;observed_raster=raster;observed_visibility=visibility;}
uint32_t recomp_load32(Ctx*,uint64_t at){uint32_t out;std::memcpy(&out,reinterpret_cast<void*>(uintptr_t(at)),4);return out;}
void a64_vset64(unsigned char* out,uint64_t value){std::memcpy(out,&value,8);}
void recomp_store32(Ctx*,uint64_t at,uint32_t value){std::memcpy(reinterpret_cast<void*>(uintptr_t(at)),&value,4);}
void hos_vr_lens_parameters(Ctx *c, uint64_t projection, float tangent, float aspect) {
    float fields[5];if(!projection||!host_vr_lens_parameters(tangent,aspect,fields))return;
    for(int i=0;i<5;++i){uint32_t bits;memcpy(&bits,&fields[i],4);recomp_store32(c,projection+0xa0+4*i,bits);}
}

void patch(Ctx* c){SITE}
void check_cache(const unsigned char* object,float expected){float f[5];std::memcpy(f,object+0xa0,sizeof f);if(std::abs(f[0]-2*std::atan(expected))>1e-6||std::abs(f[1]/f[2]-expected)>1e-6||std::abs(f[1]*f[1]+f[2]*f[2]-1)>1e-6)throw std::runtime_error("stale angle/sine/cosine lens cache");}
float lane(Ctx& c,int reg){float out;std::memcpy(&out,c.vreg[reg],4);return out;}
void input(Ctx& c,unsigned char* object,float fov,float ratio){c.x[BASE]=uintptr_t(object);std::memcpy(c.vreg[0],&fov,4);std::memcpy(c.vreg[RATIO],&ratio,4);}
void check(bool b){if(!b)throw std::runtime_error("lens hook register/culling mismatch");}
int main(){Ctx c;alignas(16) unsigned char object[512]{};input(c,object,.7f,16.f/9);patch(&c);check(lane(c,0)==.7f&&lane(c,RATIO)==16.f/9);
hos_vr_fov=1.1917536f;hos_vr_aspect=.9f;patch(&c);check(lane(c,0)==1.1917536f&&lane(c,RATIO)==.9f);float f,a;std::memcpy(&f,object+0xac,4);std::memcpy(&a,object+0xb0,4);check(f==1.1917536f&&a==.9f);check_cache(object,1.1917536f);
hos_vr_cull_tangent=5.8f;input(c,object,.7f,16.f/9);patch(&c);std::memcpy(&f,object+0xac,4);check(lane(c,0)==1.1917536f&&f==5.8f&&observations==1&&observed_raster==1.1917536f&&observed_visibility==5.8f);check_cache(object,5.8f);
hos_vr_cull_tangent=20;input(c,object,.7f,16.f/9);patch(&c);std::memcpy(&f,object+0xac,4);check(lane(c,0)==1.1917536f&&f==1.1917536f&&observations==1);hos_vr_cull_tangent=0;
input(c,object,.7f,1.f);patch(&c);check(lane(c,0)==.7f&&lane(c,RATIO)==1.f);
// The picture-ratio hook may have converted the main camera before this site.
hos_wide_ratio=1.25f;input(c,object,.7f,16.f/9);patch(&c);check(lane(c,0)==1.1917536f&&lane(c,RATIO)==.9f);
std::memcpy(&f,object+0xac,4);std::memcpy(&a,object+0xb0,4);check(f==1.1917536f&&a==.9f);check_cache(object,1.1917536f);
input(c,object,.7f,1.25f);patch(&c);check(lane(c,0)==1.1917536f&&lane(c,RATIO)==.9f);
hos_wide_ratio=0;input(c,object,.7f,1.25f);patch(&c);check(lane(c,0)==.7f&&lane(c,RATIO)==1.25f);
input(c,object,.7f,16.f/9);hos_vr_fov=3.2f;patch(&c);check(lane(c,0)==.7f);
hos_vr_fov=0;hos_vr_aspect=0;hos_wide_ratio=2.3f;patch(&c);check(lane(c,RATIO)==2.3f&&lane(c,0)==.7f);return 0;}
"""
with tempfile.TemporaryDirectory(prefix='hookshot-lens-') as temp:
    for reg,base in ((1,0),(3,21),(3,0),(1,2)):
        source=Path(temp)/f'fixture-{reg}-{base}.cpp';exe=source.with_suffix('.exe')
        source.write_text(fixture.replace('VIEW_HEADER',(ROOT/'host/vr_view_parameters.h').as_posix()).replace('SITE',oot_wide._ratio(reg,base)).replace('BASE',str(base)).replace('RATIO',str(reg)),encoding='utf-8')
        subprocess.run([args.cxx,'-std=c++17','-Wall','-Wextra','-Werror',*(['-static'] if os.name=='nt' else []),str(source),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=10)

    setup=fixture.split('int main(){',1)[0]+r"""
int main(){Ctx c;alignas(16) unsigned char object[512]{};unsigned char original[512]{};
input(c,object,.7f,16.f/9);patch(&c);check(lane(c,0)==16.f/9&&!std::memcmp(object,original,sizeof object));
hos_vr_fov=2.28f;hos_vr_aspect=.96375f;input(c,object,.7f,16.f/9);patch(&c);check(lane(c,0)==hos_vr_aspect);check_cache(object,2.28f);
float aspect;std::memcpy(&aspect,object+0xb0,4);check(aspect==hos_vr_aspect);
// Model the engine resetting its scalar cache from the body camera next tick.
const float narrow[5]={.7f,std::sin(.35f),std::cos(.35f),std::tan(.35f),16.f/9};std::memcpy(object+0xa0,narrow,sizeof narrow);
input(c,object,.7f,16.f/9);patch(&c);check_cache(object,2.28f);check(lane(c,0)==hos_vr_aspect);
hos_vr_fov=0;input(c,object,.7f,16.f/9);std::memcpy(original,object,sizeof object);patch(&c);check(!std::memcmp(object,original,sizeof object));return 0;}
"""
    source=Path(temp)/'camera-setup.cpp';exe=source.with_suffix('.exe')
    source.write_text(setup.replace('VIEW_HEADER',(ROOT/'host/vr_view_parameters.h').as_posix()).replace('SITE',next(site for offset,word,site in oot_wide.SITES if offset==0x4c5334)).replace('BASE','20').replace('RATIO','0'),encoding='utf-8')
    subprocess.run([args.cxx,'-std=c++17','-Wall','-Wextra','-Werror',*(['-static'] if os.name=='nt' else []),str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=10)
print('Perspective hook updates complete scalar cache at early camera setup and all matrix sites; default/other projections preserved.')
