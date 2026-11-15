// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
namespace mkwvk {
struct CameraPayloadHistorySample {
    uint64_t frame=0,program=0,recording=0,revision=0,source=0;
    uint32_t width=0,height=0;
    size_t bytes=0;
};
// Bounded read-only diagnostic policy. Samples are submission-overlay copies,
// not GPU-completed readback or proof of temporal/shader semantics.
class CameraPayloadHistoryPolicy {
    uint64_t program_=0,last_frame_=0;
    uint32_t width_=0,height_=0;
    size_t bytes_=0,count_=0;
public:
    static constexpr size_t maximum_samples=32;
    bool wants(uint64_t frame,uint64_t program,size_t bytes)const{
        return frame&&program&&bytes>=0xb00&&bytes<=65536&&bytes%4==0&&count_<maximum_samples&&
               (!count_||(program==program_&&bytes==bytes_&&frame>last_frame_));
    }
    bool accept(const CameraPayloadHistorySample& s){
        if(!wants(s.frame,s.program,s.bytes)||!s.recording||!s.revision||!s.source||!s.width||!s.height||
           (count_&&(s.width!=width_||s.height!=height_)))return false;
        if(!count_){program_=s.program;bytes_=s.bytes;width_=s.width;height_=s.height;}
        last_frame_=s.frame;++count_;return true;
    }
    size_t count()const{return count_;}
};
}
