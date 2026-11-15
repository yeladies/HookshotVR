"""Compile actual host observation functions with authored memory/counter stubs."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import sys
ROOT=Path(__file__).resolve().parents[2]


def function(source,signature):
    start=source.index(signature)
    return source[start:source.index('\n}\n',start)+3]


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--compiler',required=True);args=parser.parse_args()
    source=(ROOT/'host/rx_backend.c').read_text()
    code=r"""
#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include "vk_api.h"
#include "vr_camera_read.h"
using SRWLOCK=int;using LONG=int32_t;
#define SRWLOCK_INIT 0
#define HOS_OPT_CHEAT_CAMERA 1
void AcquireSRWLockExclusive(SRWLOCK*){} void ReleaseSRWLockExclusive(SRWLOCK*){}
void AcquireSRWLockShared(SRWLOCK*){} void ReleaseSRWLockShared(SRWLOCK*){}
static volatile LONG presents=100;
static SRWLOCK camera_observation_lock=0,camera_cache_lock=0;
static uint64_t camera_observation_sequence=0;
static HostVrCameraCache camera_cache{};
static uint64_t camera_uniform_candidates[16]{};static size_t camera_uniform_candidates_count=0;
MkwVkRenderer renderer{};MkwVkRenderer* vk=&renderer;
bool first_person=true,advance_on_primary=false;
std::vector<MkwVkCameraObservation> observations;
unsigned char guest[65536]{};
#define GP(a) (guest+(a))
bool env_on(const char* name){return !std::strcmp(name,"HOS_VR_FIRST_PERSON")?first_person:true;}
LONG InterlockedCompareExchange(volatile LONG* value,LONG,LONG){return *value;}
int64_t hos_now_ns(){static int64_t value=1;return value+=1000000000;}
int rx_option(int,int){return 0;}
static int camera_read_guest(uint64_t address,void* out,size_t bytes,void*){
    if(address==0x84ca6bc0ULL&&bytes==8){uint64_t singleton=0x1000;std::memcpy(out,&singleton,8);return 1;}
    if(!address||address>sizeof guest||bytes>sizeof guest-address)return 0;
    std::memcpy(out,GP(address),bytes);return 1;
}
void note(const MkwVkCameraObservation* sample){observations.push_back(*sample);if(advance_on_primary&&(sample->flags&MKWVK_CAMERA_PRIMARY))++presents;}
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class T>void put(uint64_t at,T value){std::memcpy(GP(at),&value,sizeof value);}
"""
    for signature in ('static void camera_observe_at(', 'static void camera_observe(Mkw', 'static void camera_cache_primary(',
                      'static int64_t camera_cached_present(', 'static void camera_cache_uniform(', 'static int64_t camera_publish_primary(',
                      'void rx_xr_camera_view(', 'void rx_vr_first_person_camera_observation(', 'static void camera_observe_uniform(',
                      'static void camera_observe_submit(void) {'):
        code+=function(source,signature)
    code+=r"""
int main(){try{
    renderer.camera_observation=note;
    constexpr uint64_t view=0x1800,uniform=0x2000;
    put<uint64_t>(0x1000,0x1100);put<uint32_t>(0x1120,1);put<uint64_t>(0x1128,0x1200);
    put<uint64_t>(0x1200,0x1300);put<uint64_t>(0x1378,view);put<uint64_t>(view,0x84c18a88ULL);
    float rows[12]{};rows[0]=rows[5]=rows[10]=1;double eye[3]{};
    std::memcpy(GP(view+8),rows,sizeof rows);std::memcpy(GP(view+96),eye,sizeof eye);std::memcpy(GP(uniform),rows,sizeof rows);
    float projection[16]{};projection[0]=projection[5]=1;projection[10]=-1.01f;projection[11]=-.2f;projection[14]=-1;
    std::memcpy(GP(uniform+0x1a8),projection,sizeof projection);
    rx_vr_first_person_camera_observation(view,rows,eye,41);
    check(observations.back().game_present==41,"first-person writer tag");
    presents=777;rx_xr_camera_view(view,rows,eye,0);
    check(observations.back().game_present==41,"generic observer cannot refresh writer tag");
    camera_observe_uniform(uniform+0x1a8,projection);
    check(observations.back().game_present==41,"uniform observer retains writer tag");
    observations.clear();advance_on_primary=true;camera_observe_submit();advance_on_primary=false;
    check(observations.size()==2,"submit has paired primary and uniform");
    for(const auto& item:observations)check(item.game_present==41,"submit freezes the writer tag across counter changes");
    rows[3]=.00001f;std::memcpy(GP(view+8),rows,sizeof rows);std::memcpy(GP(uniform),rows,sizeof rows);
    observations.clear();camera_observe_submit();
    check(!observations.empty(),"changed view observed");for(const auto& item:observations)check(item.game_present==-1,"changed close view must not borrow a frame");
    first_person=false;presents=900;observations.clear();camera_observe_submit();
    for(const auto& item:observations)check(item.game_present==900,"ordinary camera retains counter behavior");
    std::puts("Writer-owned camera tags survive actual uniform and submit observers");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
"""
    with tempfile.TemporaryDirectory(prefix='camera-owner-',dir=ROOT/'work') as directory:
        root=Path(directory);src=root/'fixture.cpp';exe=root/'fixture.exe';src.write_text(code)
        # The extracted production C uses the valid C zero initializer {0}.
        subprocess.run([args.compiler,*(['-static'] if sys.platform=='win32' else []),'-std=c++17','-O1','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-I',str(ROOT/'host'),'-I',str(ROOT/'gpu/vk'),str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True,timeout=20)


if __name__=='__main__':main()
