// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
namespace mkwvk {
// A skipped original draw must never lend its prepared resources to the next draw.
// Values are native queue recording values, not frame numbers or CPU counters.
class XrBaselineAttempt {
public:
    enum class Phase {Idle,Prepared,Finished,Abandoned,Retained,Retired};
private:
    Phase phase_=Phase::Idle;
    uint64_t serial_=0,recording_=0,program_=0,device_=0,command_=0;
public:
    bool prepare(uint64_t serial,uint64_t recording,uint64_t program,uint64_t device,uint64_t command){
        if(phase_!=Phase::Idle||!serial||!recording||!program||!device||!command)return false;
        serial_=serial;recording_=recording;program_=program;device_=device;command_=command;
        phase_=Phase::Prepared;return true;
    }
    bool finish(uint64_t serial,uint64_t recording,uint64_t program,uint64_t device,uint64_t command){
        if(phase_!=Phase::Prepared||serial!=serial_||recording!=recording_||program!=program_||device!=device_||command!=command_)return false;
        phase_=Phase::Finished;return true;
    }
    void next_draw(uint64_t serial){if(phase_==Phase::Prepared&&serial!=serial_)phase_=Phase::Abandoned;}
    void abandon(){if(phase_==Phase::Prepared)phase_=Phase::Abandoned;}
    bool retire(uint64_t device,uint64_t completed,bool completion_known){
        if(phase_!=Phase::Finished&&phase_!=Phase::Abandoned)return false;
        if(device!=device_||!completion_known){phase_=Phase::Retained;return false;}
        if(completed<recording_)return false;
        phase_=Phase::Retired;return true;
    }
    Phase phase()const{return phase_;}
    uint64_t recording()const{return recording_;}
    bool completed_draw()const{return phase_==Phase::Finished;}
};
struct XrBaselineComparison {
    enum class Result {Invalid,Invisible,Match,Mismatch};
    Result result=Result::Invalid;
    bool matrix_probe_requested=false,matrix_probe_validated=false,matrix_source_unchanged=false;
    bool depth_reference=false;
    bool shifted_draw_requested=false,shifted_draw_verified=false,view_shift_requested=false,view_color_requested=false;float view_shift_units=0;
    std::array<double,3> shifted_centroid_x{},shifted_centroid_y{};std::array<uint64_t,3> shifted_changed_pixels{};
    bool root_alias_requested=false,root_alias_verified=false;uint64_t root_alias_allocated_bytes=0;
    std::array<uint64_t,2> root_alias_counts{},root_alias_uncovered{},root_alias_errors{};
    uint64_t matrix_payload_bytes=0;
    uint64_t changed_pixels=0;
    std::array<uint64_t,2> mismatched_pixels{};
};
// Four tightly packed B10G11R11 frames: original before/after and private left/right.
// Bitwise equality is intentional: this first experiment changes no camera data.
inline XrBaselineComparison compare_xr_baseline(const void* bytes,size_t available,uint32_t width,uint32_t height){
    XrBaselineComparison out;
    if(!bytes||!width||!height||width>4096||height>4096)return out;
    const uint64_t pixels=uint64_t(width)*height,frame=pixels*4;
    if(available!=frame*4)return out;
    const auto* data=static_cast<const unsigned char*>(bytes);
    for(uint64_t pixel=0;pixel<pixels;++pixel){
        const auto offset=pixel*4;
        out.changed_pixels+=std::memcmp(data+offset,data+frame+offset,4)!=0;
        for(unsigned eye=0;eye<2;++eye)out.mismatched_pixels[eye]+=std::memcmp(data+frame+offset,data+(eye+2)*frame+offset,4)!=0;
    }
    // A mismatch is never hidden by an unchanged original reference.
    out.result=(out.mismatched_pixels[0]||out.mismatched_pixels[1])?XrBaselineComparison::Result::Mismatch:
        out.changed_pixels?XrBaselineComparison::Result::Match:XrBaselineComparison::Result::Invisible;
    return out;
}
}
