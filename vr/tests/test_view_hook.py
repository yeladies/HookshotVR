# SPDX-License-Identifier: GPL-3.0-or-later
"""Execute the actual guarded gameplay hook against an authored guest view."""
from pathlib import Path
import argparse, subprocess, tempfile, unittest
ROOT=Path(__file__).resolve().parents[2]
class ViewHook(unittest.TestCase):
    def test_actual_hook_updates_only_primary_derived_view(self):
        source=(ROOT/'host/hos_main.c').read_text()
        start=source.index('void hos_oot_free_view(Ctx *c, uint64_t view) {')
        end=source.index('\n}\n',start)+3
        hook=source[start:end]
        prefix=r"""
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cstdarg>
#include <stdexcept>
#include "VIEW_HEADER"
struct Ctx {};
using SRWLOCK=int;
#define SRWLOCK_INIT 0
#define HOS_OPT_CHEAT_CAMERA 1
void AcquireSRWLockExclusive(SRWLOCK*){} void ReleaseSRWLockExclusive(SRWLOCK*){}
constexpr uint64_t address=0x10000;alignas(8) unsigned char memory[256];bool primary=true,immersive=true,coherent=true;int debug=0,overrides=0;
void* ptr(uint64_t a){if(a<address||a>=address+sizeof memory)throw std::runtime_error("unexpected guest access");return memory+a-address;}
#define A64_PTR(a) ptr(a)
uint64_t recomp_load64(Ctx*,uint64_t a){uint64_t b;std::memcpy(&b,ptr(a),8);return b;}
void recomp_store32(Ctx*,uint64_t a,uint32_t b){std::memcpy(ptr(a),&b,4);}void recomp_store64(Ctx*,uint64_t a,uint64_t b){std::memcpy(ptr(a),&b,8);}
uint64_t oot_gameplay_view(Ctx*){return primary?address:0;}
int rx_option(int,int){return debug;}
bool env_on(const char* s){return !std::strcmp(s,"RX_VK_XR_IMMERSIVE")?immersive:!std::strcmp(s,"HOOKSHOT_VR_COHERENT_VIEW")?coherent:false;}
void hos_log(const char*,...){}void rx_xr_camera_view(uint64_t,const float*,const double*,int){}void oot_build_free_view(Ctx*,uint64_t,uint64_t){}
int rx_xr_head_camera(uint64_t,float rows[12]){++overrides;const float a=float(std::sqrt(.5));float out[12]={a,0,-a,20*a,0,1,0,-20,a,0,a,-40*a};std::memcpy(rows,out,sizeof out);return 1;}
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void reset(){std::memset(memory,0,sizeof memory);uint64_t vt=0x84c18a88ULL;std::memcpy(memory,&vt,8);
 const float view[]={1,0,0,-10,0,1,0,-20,0,0,1,-30},cache[]={10,20,30,10,20,25,0,1,0};const double eye[]={10,20,30},target[]={10,20,25};
 std::memcpy(memory+8,view,sizeof view);std::memcpy(memory+56,cache,sizeof cache);std::memcpy(memory+96,eye,sizeof eye);std::memcpy(memory+120,target,sizeof target);}
""".replace('VIEW_HEADER',(ROOT/'host/vr_view_parameters.h').as_posix())
        suffix=r"""
int main(){try{Ctx ctx;reset();unsigned char original[256];std::memcpy(original,memory,sizeof memory);
 immersive=false;hos_oot_free_view(&ctx,address);check(!std::memcmp(original,memory,sizeof memory)&&overrides==0,"default-off view untouched");
 immersive=true;primary=false;hos_oot_free_view(&ctx,address);check(!std::memcmp(original,memory,sizeof memory)&&overrides==0,"non-primary view untouched");primary=true;
 coherent=false;hos_oot_free_view(&ctx,address);check(std::memcmp(original+8,memory+8,48)&&!std::memcmp(original+56,memory+56,88),"legacy comparison changes only matrix");
 reset();coherent=true;hos_oot_free_view(&ctx,address);float view[12],cache[9];double eye[3],target[3];std::memcpy(view,memory+8,48);std::memcpy(cache,memory+56,36);std::memcpy(eye,memory+96,24);std::memcpy(target,memory+120,24);
 for(int k=0;k<3;++k){check(std::abs((eye[k]-target[k])/5-view[8+k])<1e-5,"actual hook target must track head view");check(std::abs(cache[3+k]-target[k])<1e-4&&std::abs(cache[6+k]-view[4+k])<1e-5,"actual hook synchronizes float cache");}
 check(!std::memcmp(original+144,memory+144,112),"unrelated view fields untouched");
 return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
"""
        with tempfile.TemporaryDirectory(prefix='vr-view-hook-') as folder:
            root=Path(folder);cpp=root/'test.cpp';exe=root/'test.exe';cpp.write_text(prefix+hook+suffix,encoding='utf-8')
            built=subprocess.run([ARGS.compiler,'-std=c++20','-O1','-static',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=40)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10);self.assertEqual(run.returncode,0,run.stdout+run.stderr)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--compiler',required=True);ARGS,rest=parser.parse_known_args();unittest.main(argv=[__file__,*rest])
