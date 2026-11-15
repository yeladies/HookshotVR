// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_history_capture.h"
#include <cstdio>
#include <stdexcept>
static void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{
    mkwvk::CameraPayloadHistoryPolicy policy;
    mkwvk::CameraPayloadHistorySample s{1,2,3,4,5,1280,720,0x3500};
    check(policy.wants(1,2,s.bytes)&&policy.accept(s)&&policy.count()==1,"first scoped sample");
    check(!policy.accept(s)&&!policy.wants(1,2,s.bytes),"one sample per replay frame");
    s.frame=2;s.program=9;check(!policy.accept(s),"program identity fixed");s.program=2;
    s.bytes=0x3600;check(!policy.accept(s),"range fixed");s.bytes=0x3500;
    s.width=720;check(!policy.accept(s),"viewport fixed");s.width=1280;
    s.revision=0;check(!policy.accept(s),"accepted scene-camera provenance required");s.revision=4;
    for(unsigned k=1;k<32;++k){s.frame=k+1;check(policy.accept(s),"bounded consecutive samples");}
    s.frame=33;check(!policy.accept(s)&&policy.count()==32,"capture stops at 32");
    mkwvk::CameraPayloadHistoryPolicy empty;
    check(!empty.wants(0,2,0x3500)&&!empty.wants(1,0,0x3500)&&!empty.wants(1,2,1024)&&!empty.wants(1,2,65540),"invalid captures rejected before allocation");
    std::puts("Camera payload history capture policy passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
