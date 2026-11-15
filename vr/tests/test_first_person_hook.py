"""Compile and execute actual host writer against authored guest memory."""
from pathlib import Path
import argparse
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[2]

def function(source,signature):
    start=source.index(signature)
    return source[start:source.index('\n}\n',start)+3]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--compiler',required=True);args=parser.parse_args()
    source=(ROOT/'host/hos_main.c').read_text()
    code=r"""
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <cstdio>
#include "vk_api.h"
struct Ctx{};
using SRWLOCK=int;
#define SRWLOCK_INIT 0
#define HOS_OPT_CHEAT_CAMERA 1
void AcquireSRWLockExclusive(SRWLOCK*){} void ReleaseSRWLockExclusive(SRWLOCK*){}
void AcquireSRWLockShared(SRWLOCK*){} void ReleaseSRWLockShared(SRWLOCK*){}
struct OotFirstPersonRoot {uint64_t actor;int64_t seen;double position[3],yaw;int child;};
OotFirstPersonRoot first_person_root{42,100,{10,20,30},0,1};SRWLOCK first_person_root_lock=0;
constexpr uint64_t address=0x10000;unsigned char memory[256];bool primary=true,enabled=false,valid=true,capture=false;int debug=0,calls=0,observations=0;int64_t now=200;
const char* number="1.25";
const char* fake_getenv(const char*){return number;}
#define getenv fake_getenv
bool env_on(const char* name){return !std::strcmp(name,"HOS_VR_FIRST_PERSON")?enabled:!std::strcmp(name,"HOOKSHOT_VR_CAMERA_CAPTURE")?capture:false;}
int64_t hos_now_ns(){return now;}
void* ptr(uint64_t a,size_t n){if(a<address||a+n>address+sizeof memory)throw std::runtime_error("guest write out of bounds");return memory+a-address;}
uint64_t recomp_load64(Ctx*,uint64_t a){uint64_t b;std::memcpy(&b,ptr(a,8),8);return b;}
void recomp_store32(Ctx*,uint64_t a,uint32_t b){std::memcpy(ptr(a,4),&b,4);}
void recomp_store64(Ctx*,uint64_t a,uint64_t b){std::memcpy(ptr(a,8),&b,8);}
uint64_t oot_gameplay_view(Ctx*){return primary?address:0;}
int rx_option(int,int){return debug;}
void oot_build_free_view(Ctx*,uint64_t,uint64_t){}
void hos_log(const char*,...){}
void rx_xr_camera_view(uint64_t,const float*,const double*,int){++observations;}
int rx_vr_first_person_frame(MkwVkFirstPersonInput* i,MkwVkFirstPersonFrame* o){++calls;if(!valid)return 0;
 if(i->root_position[0]!=10||i->actor_id!=42)throw std::runtime_error("wrong root input");
 for(unsigned k=0;k<12;++k)o->head_view[k]=float(k+1);
 for(unsigned k=0;k<9;++k)o->eye_target_up[k]=float(k+101);
 for(unsigned k=0;k<3;++k){o->eye_position[k]=k+1001;o->look_target[k]=k+2001;}return 1;}
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
void reset(){std::memset(memory,0x5a,sizeof memory);uint64_t vt=0x84c18a88ULL;std::memcpy(memory,&vt,8);}
"""
    code+=function(source,'static int oot_first_person_number(')
    code+=function(source,'static int oot_build_first_person_view(')
    code+=function(source,'void hos_oot_free_view(')
    code+=r"""
int main(){try{Ctx c;reset();unsigned char saved[256];std::memcpy(saved,memory,256);
 hos_oot_free_view(&c,address);check(!std::memcmp(memory,saved,256)&&!calls,"default off");
 enabled=true;primary=false;hos_oot_free_view(&c,address);check(!std::memcmp(memory,saved,256)&&!calls,"non-primary view");primary=true;
 debug=1;hos_oot_free_view(&c,address);check(!std::memcmp(memory,saved,256)&&!calls,"debug camera preserved");debug=0;
 valid=false;hos_oot_free_view(&c,address);check(!std::memcmp(memory,saved,256)&&calls==1,"failed tracking leaves guest memory intact");valid=true;
 number="nan";hos_oot_free_view(&c,address);check(calls==1,"invalid scale rejected before callback");number="1.25x";hos_oot_free_view(&c,address);check(calls==1,"trailing text rejected");number="1.25";
 now=300000101;hos_oot_free_view(&c,address);check(calls==1,"stale root rejected");now=200;
 capture=true;hos_oot_free_view(&c,address);check(calls==2&&observations==1,"valid view and observation");
 for(unsigned k=0;k<12;++k){float v;std::memcpy(&v,memory+8+4*k,4);check(v==float(k+1),"view rows");}
 for(unsigned k=0;k<9;++k){float v;std::memcpy(&v,memory+56+4*k,4);check(v==float(k+101),"float caches");}
 for(unsigned k=0;k<3;++k){double v;std::memcpy(&v,memory+96+8*k,8);check(v==k+1001,"double eye");std::memcpy(&v,memory+120+8*k,8);check(v==k+2001,"double target");}
 check(!std::memcmp(memory,saved,8)&&!std::memcmp(memory+92,saved+92,4)&&!std::memcmp(memory+144,saved+144,112),"unrelated fields unchanged");
 std::puts("Actual first-person host writer guards and coherent caches passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
"""
    with tempfile.TemporaryDirectory(prefix='vr-first-person-hook-') as directory:
        path=Path(directory);cpp=path/'hook.cpp';exe=path/'hook.exe';cpp.write_text(code)
        subprocess.run([args.compiler,*(['-static'] if sys.platform=='win32' else []),'-std=c++20','-Wall','-Wextra','-Werror','-I',str(ROOT/'gpu/vk'),str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=20)

if __name__=='__main__':main()
