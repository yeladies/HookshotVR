// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_uniform_camera_scan.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mkwvk;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try {
    constexpr int64_t t=1000000000;SceneSubmissionSnapshot snapshot{};snapshot.captured_ns=t;snapshot.count=1;
    auto& c=snapshot.cameras[0];c.received_ns=t;c.pair.revision=1;c.pair.primary.camera_id=7;
    auto* v=c.pair.primary.view;v[0]=v[5]=v[10]=1;v[3]=-2;v[7]=-3;v[11]=-4;
    auto* p=c.pair.uniforms.projection;p[0]=1.2f;p[5]=1.7f;p[10]=-1.01f;p[11]=-.2f;p[14]=-1;
    std::vector<float> data(512,std::numeric_limits<float>::quiet_NaN());
    std::copy_n(v,12,data.data()+0x100/4);std::copy_n(p,16,data.data()+0x200/4);
    for(int r=0;r<4;++r)for(int col=0;col<4;++col){double val=0;for(int k=0;k<4;++k)val+=double(p[4*r+k])*(k==3?(col==3?1:0):v[4*k+col]);data[0x300/4+4*r+col]=float(val);}
    auto scan=scan_submitted_uniform(&snapshot,t+1,data.data(),data.size());
    check(scan.eligible&&!scan.truncated&&scan.view_offsets==std::vector<uint32_t>{0x100}&&scan.projection_offsets==std::vector<uint32_t>{0x200}&&scan.vp_offsets==std::vector<uint32_t>{0x300},"shifted matrix candidates found without canonical layout");
    check(!scan_submitted_uniform(&snapshot,t+600000000,data.data(),data.size()).eligible,"stale context rejected");
    check(!scan_submitted_uniform(nullptr,t+1,data.data(),data.size()).eligible,"no latest-camera fallback");
    check(!scan_submitted_uniform(&snapshot,t-1,data.data(),data.size()).eligible,"clock reversal rejected");
    check(!scan_submitted_uniform(&snapshot,t+1,data.data(),16385).eligible,"bounded read length");
    snapshot.count=2;snapshot.cameras[1]=c;snapshot.cameras[1].pair.primary.camera_id=8;
    scan=scan_submitted_uniform(&snapshot,t+1,data.data(),data.size());
    check(scan.ambiguous_offsets>=3&&scan.view_offsets.empty()&&scan.vp_offsets.empty(),"ambiguous identities cannot produce owned matrix candidates");
    snapshot.count=1;c.received_ns=t-300000000;
    check(!scan_submitted_uniform(&snapshot,t+1,data.data(),data.size()).eligible,"capture-time age bound retained");c.received_ns=t;
    std::vector<float> repeated(1200);for(size_t i=0;i<100;++i)std::copy_n(v,12,repeated.data()+i*12);
    scan=scan_submitted_uniform(&snapshot,t+1,repeated.data(),repeated.size());
    check(scan.truncated&&scan.view_offsets.size()==64,"candidate output bounded");
    std::cout<<"Submitted uniform candidate search preserves identity, age, alignment and output bounds\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
