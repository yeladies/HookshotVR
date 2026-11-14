// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_gpu_frame_diagnostics.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{
    GpuFrameDiagnostics d;using R=GpuFrameReason;const int64_t ms=1000000;
    d.observe(false,R::SourceUnavailable,R::None,100*ms);d.observe(false,R::SourceUnavailable,R::GpuPending,110*ms);
    check(d.startup==2&&d.omitted==0&&d.gaps==0,"loading is not a blackout after gameplay");
    d.observe(true,R::None,R::None,120*ms);d.observe(false,R::SourceStale,R::GpuPending,150*ms);d.observe(false,R::SourceStale,R::ImagePending,250*ms);
    check(d.omitted==2&&d.gaps==1&&d.current_gap_ns==100*ms,"consecutive missing layers form one gap");
    d.observe(true,R::None,R::None,350*ms);check(d.max_gap_ns==200*ms&&d.current_gap_ns==0,"restoration closes gap at observed return");
    d.observe(false,R::OriginChanged,R::None,400*ms);d.observe(false,R::RuntimeHidden,R::None,420*ms);
    const auto frames=d.frames;d.observe(true,R::None,R::None,410*ms);check(d.frames==frames,"clock reversal ignored");
    check(d.layers+d.startup+d.omitted==d.frames&&d.gaps==2,"all submitted frame outcomes accounted");
    check(d.reasons[unsigned(R::SourceStale)]==2&&d.blockers[unsigned(R::GpuPending)]==2,"invalid previous image and blocked new job are separate facts");
    std::ostringstream out;d.json(out);check(out.str().find("\"pixel_blackness_measured\":false")!=std::string::npos,"metadata does not claim pixel measurement");
    std::cout<<"Startup, missing layers, concurrent blockers and blackout duration accounted\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
