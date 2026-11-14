// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <ostream>
namespace hookshot::vr {
// CPU monotonic observation ages at successful xrEndFrame return. These are
// not GPU timestamps, compositor display times or motion-to-photon latency.
struct GpuPresentationTiming {
    uint64_t samples=0,reused_pairs=0,pair_runs=0,invalid_samples=0;
    uint64_t source_age_sum_ns=0,source_age_max_ns=0,eye_age_sum_ns=0,eye_age_max_ns=0,ready_age_sum_ns=0,ready_age_max_ns=0;
    uint64_t source_to_eye_sum_ns=0,eye_to_ready_sum_ns=0;
    std::array<uint64_t,6> eye_age_buckets{};
    uint64_t previous_pair=0,previous_generation=0,previous_origin=0;
    bool record(uint64_t pair,uint64_t source,uint64_t generation,uint64_t origin,int64_t source_observed,int64_t eye_observed,int64_t ready,int64_t submitted){
        if(!pair||!source||!generation||!origin||source_observed<=0||eye_observed<source_observed||ready<eye_observed||submitted<ready||submitted-source_observed>5000000000ll){++invalid_samples;return false;}
        const uint64_t a=uint64_t(submitted-source_observed),b=uint64_t(submitted-eye_observed),c=uint64_t(submitted-ready);
        if(pair==previous_pair&&generation==previous_generation&&origin==previous_origin)++reused_pairs;else ++pair_runs;
        previous_pair=pair;previous_generation=generation;previous_origin=origin;++samples;
        source_age_sum_ns+=a;eye_age_sum_ns+=b;ready_age_sum_ns+=c;
        if(a>source_age_max_ns)source_age_max_ns=a;if(b>eye_age_max_ns)eye_age_max_ns=b;if(c>ready_age_max_ns)ready_age_max_ns=c;
        source_to_eye_sum_ns+=uint64_t(eye_observed-source_observed);eye_to_ready_sum_ns+=uint64_t(ready-eye_observed);
        unsigned bucket=0;for(uint64_t limit:{8000000ull,16000000ull,33000000ull,66000000ull,100000000ull}){if(b<=limit)break;++bucket;}++eye_age_buckets[bucket];return true;
    }
    void json(std::ostream& out)const{
        out<<"{\"clock\":\"CPU_steady_observations_at_xrEndFrame_return\",\"motion_to_photon_measurement\":false,\"samples\":"<<samples<<",\"reused_pairs\":"<<reused_pairs<<",\"pair_runs\":"<<pair_runs<<",\"invalid_samples\":"<<invalid_samples
           <<",\"source_pose_age_sum_ns\":"<<source_age_sum_ns<<",\"source_pose_age_max_ns\":"<<source_age_max_ns<<",\"eye_pose_age_sum_ns\":"<<eye_age_sum_ns<<",\"eye_pose_age_max_ns\":"<<eye_age_max_ns
           <<",\"pair_ready_age_sum_ns\":"<<ready_age_sum_ns<<",\"pair_ready_age_max_ns\":"<<ready_age_max_ns<<",\"source_to_eye_sum_ns\":"<<source_to_eye_sum_ns<<",\"eye_to_pair_ready_sum_ns\":"<<eye_to_ready_sum_ns
           <<",\"eye_age_upper_bounds_ns\":[8000000,16000000,33000000,66000000,100000000,null],\"eye_age_counts\":[";
        for(unsigned i=0;i<eye_age_buckets.size();++i){if(i)out<<',';out<<eye_age_buckets[i];}out<<"]}";
    }
};
}
