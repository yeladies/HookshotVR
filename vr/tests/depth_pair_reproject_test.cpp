// SPDX-License-Identifier: GPL-3.0-or-later
#include "depth_pair_reproject.h"
#include <iostream>
#include <cstring>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
DepthPairParameters valid(){DepthPairParameters p;p.source_from_eye={1,0,0,-.032f,0,1,0,0,0,0,1,0};p.eye_tangents={-1,1,1,-1};p.projection={.8f,.8f,-1.020202f,-.2020202f};p.optical={0,0,100,1.25f};p.dimensions={80,80,64,64};p.settings={123,4,0,0};return p;}
int main(){try{
    auto p=valid();check(depth_pair_parameters_valid(p),"bounded rigid source-from-eye model");
    for(int i=0;i<13;++i){auto q=p;if(i==0)q.source_from_eye[0]=2;if(i==1)q.source_from_eye[0]=-1;if(i==2)q.source_from_eye[3]=101;
        if(i==3)q.eye_tangents[0]=0;if(i==4)q.eye_tangents[2]=-1;if(i==5)q.projection[0]=0;if(i==6)q.projection[2]=-.99f;
        if(i==7)q.optical[2]=0;if(i==8)q.dimensions[0]=1921;if(i==9)q.dimensions[2]=4097;if(i==10)q.settings[0]=124;if(i==11)q.settings[1]=17;if(i==12)q.settings[2]=1;
        check(!depth_pair_parameters_valid(q),"invalid dimensions, reflection, frustum or shader setting rejected");}
    DepthPairDispatch d{0x10000,0x20000,0x30000,0x40000};
    auto admitted=[&](auto value){return depth_pair_dispatch_valid(value,p,51200,16384,128,16);};check(admitted(d),"disjoint bounded GPU dispatch");
    auto q=d;q.counters=0;check(depth_pair_dispatch_valid(q,p,51200,16384,128,0),"production counters can be disabled without atomics");
    for(int i=0;i<5;++i){q=d;if(i==0)q.source=0;if(i==1)q.destination=q.source;if(i==2)q.parameters=q.destination;if(i==3)q.source++;if(i==4)q.counters=UINT64_MAX-15;check(!admitted(q),"invalid GPU ranges rejected");}
    check(!depth_pair_dispatch_valid(d,p,51199,16384,128,16),"short source snapshot rejected");
    check(!depth_pair_dispatch_valid(d,p,51200,16383,128,16),"short output rejected");
    check(!depth_pair_dispatch_valid(d,p,51200,16384,127,16),"short metadata rejected");
    DepthPairSource source{0x10000,51200,1,2,3,9,4,80,80,123,0,true,true};
    source.projection=p.projection;source.optical_center={p.optical[0],p.optical[1]};
    std::array<DepthPairOutput,2> eyes{{{0x20000,16384,0x30000,128,0x40000,16,5,2,10,4,0,0},{0x50000,16384,0x60000,128,0x70000,16,5,2,10,4,1,0}}};
    std::array<DepthPairParameters,2> cameras{p,p};cameras[1].source_from_eye[3]=.032f;
    std::array<DepthPairDispatch,2> dispatches{};
    check(prepare_depth_pair_dispatches(source,10,4,eyes,cameras,dispatches)&&dispatches[0].source==dispatches[1].source,"one immutable prior source supports both eyes with a newer shared pose");
    for(int fault=0;fault<11;++fault){auto ss=source;auto ee=eyes;auto pp=cameras;
        if(fault==0)ss.recording=11;if(fault==1)ss.generation=3;if(fault==2)ss.ordered_before_consumer=false;if(fault==3)ss.immutable=false;
        if(fault==4)ee[1].eye=0;if(fault==5)ee[1].pose_sample++;if(fault==6)ee[1].origin++;if(fault==7)pp[1].projection[0]=1;
        if(fault==8)ee[1].address=ee[0].parameters;if(fault==9)ee[1].queue_family=1;if(fault==10)ss.projection[0]=1;
        const auto before=dispatches;check(!prepare_depth_pair_dispatches(ss,10,4,ee,pp,dispatches)&&!std::memcmp(before.data(),dispatches.data(),sizeof dispatches),"scope, source lens, cross-eye alias or queue mismatch rejects without output changes");
    }
    std::cout<<"Depth pair admission rejects invalid lenses, transforms, extents and overlapping GPU ranges\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
