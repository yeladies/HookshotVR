// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace hookshot::vr {
// A source-camera-from-eye rigid transform in source game units. Source color
// and D32 depth must be complete, immutable planes from one captured state.
struct alignas(16) DepthPairParameters {
    std::array<float,12> source_from_eye{};
    std::array<float,4> eye_tangents{}; // left, right, up, down
    std::array<float,4> projection{}; // P00, P11, P22, P23, OpenGL -1..1 clip depth
    std::array<float,4> optical{}; // P02, P12, maximum source distance, residual in source pixels
    std::array<uint32_t,4> dimensions{}; // source width/height, target width/height
    std::array<uint32_t,4> settings{}; // native color format 122/123, iterations, reserved, reserved
};
struct DepthPairDispatch {uint64_t source=0,destination=0,parameters=0,counters=0;};
static_assert(sizeof(DepthPairParameters)==128);
static_assert(sizeof(DepthPairDispatch)==32);
inline bool depth_pair_parameters_valid(const DepthPairParameters& p){
    for(float f:p.source_from_eye)if(!std::isfinite(f))return false;
    for(const auto* fields:{&p.eye_tangents,&p.projection,&p.optical})for(float f:*fields)if(!std::isfinite(f))return false;
    const auto& d=p.dimensions;const auto& t=p.eye_tangents;const auto& q=p.projection;const auto& o=p.optical;
    if(d[0]<2||d[1]<2||d[0]>1920||d[1]>1080||d[2]<2||d[3]<2||d[2]>4096||d[3]>4096||uint64_t(d[2])*d[3]>8388608||
       t[0]>=-.0001f||t[1]<=.0001f||t[2]<=.0001f||t[3]>=-.0001f||
       q[0]<.0001f||q[1]<.0001f||q[0]>10000||q[1]>10000||q[2]>-1||q[3]>=0||
       std::abs(o[0])>4||std::abs(o[1])>4||o[2]<=0||o[2]>100000||o[3]<.05f||o[3]>2||
       (p.settings[0]!=122&&p.settings[0]!=123)||p.settings[1]<2||p.settings[1]>16||p.settings[2]||p.settings[3])return false;
    for(float f:t)if(std::abs(f)>8)return false;
    const auto& r=p.source_from_eye;
    for(int a=0;a<3;++a){if(std::abs(r[4*a+3])>o[2])return false;
        for(int b=0;b<3;++b){double dot=0;for(int k=0;k<3;++k)dot+=double(r[4*a+k])*r[4*b+k];if(std::abs(dot-(a==b))>.003)return false;}}
    const double det=r[0]*(double(r[5])*r[10]-double(r[6])*r[9])-r[1]*(double(r[4])*r[10]-double(r[6])*r[8])+r[2]*(double(r[4])*r[9]-double(r[5])*r[8]);
    return std::abs(det-1)<.003;
}
// Range admission only. The owner must prove actual allocation extents, source
// frame/pose/queue identity, generation and lifetime. No pixel data is read here.
inline bool depth_pair_dispatch_valid(const DepthPairDispatch& d,const DepthPairParameters& p,
        uint64_t source_bytes,uint64_t destination_bytes,uint64_t parameter_bytes,uint64_t counter_bytes){
    if(!depth_pair_parameters_valid(p))return false;
    const std::array<uint64_t,4> starts{d.source,d.destination,d.parameters,d.counters};
    const std::array<uint64_t,4> sizes{uint64_t(p.dimensions[0])*p.dimensions[1]*8,uint64_t(p.dimensions[2])*p.dimensions[3]*4,sizeof p,d.counters?16ull:0ull};
    if(source_bytes<sizes[0]||destination_bytes<sizes[1]||parameter_bytes<sizes[2]||counter_bytes<sizes[3])return false;
    for(unsigned i=0;i<4;++i){if(i==3&&!starts[i])continue;if(!starts[i]||starts[i]%16||starts[i]>UINT64_MAX-sizes[i])return false;
        for(unsigned j=0;j<i;++j)if(sizes[j]&&starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return false;}
    return true;
}
struct DepthPairSource {
    uint64_t address=0,bytes=0,state=0,origin=0,capture_pose=0,recording=0,generation=0;
    uint32_t width=0,height=0,format=0,queue_family=0;bool immutable=false,ordered_before_consumer=false;
    std::array<float,4> projection{};std::array<float,2> optical_center{};
};
struct DepthPairOutput {
    uint64_t address=0,bytes=0,parameters=0,parameter_bytes=0,counters=0,counter_bytes=0;
    uint64_t pose_sample=0,origin=0,recording=0,generation=0;uint32_t eye=0,queue_family=0;
};
// One source object supplies both dispatches: temporal left/right inputs cannot
// be passed here. A completed immutable source may be reused with a newer pose.
// The owner must establish the declared queue ordering and actual allocations.
inline bool prepare_depth_pair_dispatches(const DepthPairSource& source,uint64_t recording,uint64_t generation,
        const std::array<DepthPairOutput,2>& eyes,const std::array<DepthPairParameters,2>& parameters,
        std::array<DepthPairDispatch,2>& out){
    if(!recording||!generation||!source.state||!source.origin||!source.capture_pose||!source.recording||source.recording>recording||source.generation!=generation||
       !source.immutable||!source.ordered_before_consumer||!source.bytes||source.bytes>16ull*1024*1024)return false;
    std::array<DepthPairDispatch,2> next{};
    for(unsigned eye=0;eye<2;++eye){const auto& e=eyes[eye];const auto& p=parameters[eye];
        if(e.eye!=eye||!e.pose_sample||e.origin!=source.origin||e.recording!=recording||e.generation!=generation||e.queue_family!=source.queue_family||
           p.dimensions[0]!=source.width||p.dimensions[1]!=source.height||p.settings[0]!=source.format||p.projection!=source.projection||p.optical[0]!=source.optical_center[0]||p.optical[1]!=source.optical_center[1])return false;
        next[eye]={source.address,e.address,e.parameters,e.counters};
        if(!depth_pair_dispatch_valid(next[eye],p,source.bytes,e.bytes,e.parameter_bytes,e.counter_bytes))return false;
    }
    if(eyes[0].pose_sample!=eyes[1].pose_sample||parameters[0].projection!=parameters[1].projection||parameters[0].optical!=parameters[1].optical||
       parameters[0].settings!=parameters[1].settings||parameters[0].dimensions!=parameters[1].dimensions)return false;
    // Check all writable/metadata regions across eyes, not just within one
    // dispatch. No eye may overwrite the other eye's parameters or counters.
    std::array<uint64_t,7> starts{source.address,eyes[0].address,eyes[0].parameters,eyes[0].counters,eyes[1].address,eyes[1].parameters,eyes[1].counters};
    const uint64_t pixels=uint64_t(parameters[0].dimensions[2])*parameters[0].dimensions[3];
    const std::array<uint64_t,7> sizes{source.bytes,pixels*4,128,eyes[0].counters?16ull:0ull,pixels*4,128,eyes[1].counters?16ull:0ull};
    for(unsigned i=0;i<7;++i){if(!sizes[i])continue;if(starts[i]>UINT64_MAX-sizes[i])return false;
        for(unsigned j=0;j<i;++j)if(sizes[j]&&starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return false;}
    out=next;return true;
}

}
