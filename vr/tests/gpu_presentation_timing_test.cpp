// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_gpu_presentation_timing.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    GpuPresentationTiming t;
    const int64_t ms=1000000;
    check(t.record(1,7,2,3,100*ms,120*ms,140*ms,150*ms),"first completed projection timing");
    check(t.record(1,7,2,3,100*ms,120*ms,140*ms,160*ms),"reused completed projection timing");
    check(t.samples==2&&t.reused_pairs==1&&t.pair_runs==1,"repeated XR submissions are not fresh GPU pairs");
    check(t.source_age_sum_ns==110*ms&&t.eye_age_sum_ns==70*ms&&t.ready_age_sum_ns==30*ms,"independent observation age sums");
    check(t.source_to_eye_sum_ns+t.eye_to_ready_sum_ns+t.ready_age_sum_ns==t.source_age_sum_ns,"causal stages partition source observation age");
    check(t.eye_age_buckets[2]==1&&t.eye_age_buckets[3]==1,"distribution counts original production pose age on reuse");
    check(t.record(1,7,2,4,200*ms,210*ms,220*ms,230*ms),"new origin starts a different pair run");
    check(t.record(1,7,5,4,240*ms,250*ms,260*ms,270*ms)&&t.pair_runs==3,"device generation cannot count as a reused pair");
    const auto samples=t.samples;for(unsigned failure=0;failure<5;++failure){check(!t.record(failure==0?0:8,7,2,3,100*ms,failure==1?90*ms:120*ms,failure==2?110*ms:140*ms,failure==3?130*ms:failure==4?6000*ms:150*ms),"invalid identity or noncausal timing rejected");}
    check(t.samples==samples&&t.invalid_samples==5,"invalid samples do not corrupt measured sums");
    std::ostringstream out;t.json(out);check(out.str().find("motion_to_photon_measurement\":false")!=std::string::npos,"report does not claim motion-to-photon measurement");
    std::cout<<"CPU observation age decomposition, original-pose reuse, origin/generation identity and invalid-sample rejection passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
