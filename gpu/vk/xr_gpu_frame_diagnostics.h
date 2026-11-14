// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace hookshot::vr {
enum class GpuFrameReason : unsigned {None,SourceUnavailable,SourceStale,InvalidSource,Tracking,OriginChanged,InvalidPair,GpuPending,ImagePending,RuntimeHidden,Suppressed,Failure,Count};
inline const char* gpu_frame_reason_name(GpuFrameReason reason){
    constexpr const char* names[]={"none","source_unavailable","source_stale","invalid_source","tracking","origin_changed","invalid_pair","gpu_pending","image_pending","runtime_hidden","suppressed","failure"};
    return unsigned(reason)<unsigned(GpuFrameReason::Count)?names[unsigned(reason)]:"invalid_reason";
}
// Counts layers actually accepted by xrEndFrame, not the colour of their pixels.
// Startup is separated from gaps after the first successfully submitted layer.
struct GpuFrameDiagnostics {
    uint64_t frames=0,layers=0,startup=0,omitted=0,gaps=0,max_gap_ns=0,current_gap_ns=0;
    std::array<uint64_t,unsigned(GpuFrameReason::Count)> reasons{},blockers{};
    int64_t gap_start=0,last_observed=0;bool seen_layer=false,last_layer=false;
    void observe(bool layer,GpuFrameReason reason,GpuFrameReason blocker,int64_t now){
        if(now<=0||now<last_observed)return;last_observed=now;++frames;
        if(layer){++layers;seen_layer=true;if(gap_start){current_gap_ns=uint64_t(now-gap_start);if(current_gap_ns>max_gap_ns)max_gap_ns=current_gap_ns;}gap_start=0;current_gap_ns=0;}
        else{
            if(unsigned(reason)>=unsigned(GpuFrameReason::Count)||reason==GpuFrameReason::None)reason=GpuFrameReason::InvalidPair;
            ++reasons[unsigned(reason)];if(unsigned(blocker)<unsigned(GpuFrameReason::Count)&&blocker!=GpuFrameReason::None)++blockers[unsigned(blocker)];
            if(!seen_layer)++startup;
            else{++omitted;if(!gap_start){gap_start=now;++gaps;}current_gap_ns=uint64_t(now-gap_start);if(current_gap_ns>max_gap_ns)max_gap_ns=current_gap_ns;}
        }
        last_layer=layer;
    }
    void json(std::ostream& out)const{
        out<<"{\"pixel_blackness_measured\":false,\"frames\":"<<frames<<",\"layers\":"<<layers<<",\"startup_without_layer\":"<<startup<<",\"omitted_after_first_layer\":"<<omitted<<",\"gap_runs\":"<<gaps<<",\"maximum_observed_gap_ns\":"<<max_gap_ns<<",\"current_observed_gap_ns\":"<<current_gap_ns<<",\"reasons\":{";
        for(unsigned i=1;i<reasons.size();++i){if(i>1)out<<',';out<<'"'<<gpu_frame_reason_name(GpuFrameReason(i))<<"\":"<<reasons[i];}out<<"},\"blocked_new_pair\":{";
        for(unsigned i=1;i<blockers.size();++i){if(i>1)out<<',';out<<'"'<<gpu_frame_reason_name(GpuFrameReason(i))<<"\":"<<blockers[i];}out<<"}}";
    }
};
}
