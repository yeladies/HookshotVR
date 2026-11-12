// SPDX-License-Identifier: GPL-3.0-or-later
#define _Static_assert static_assert
#include "xr_camera_capture.h"
#include <limits>
#include <cstddef>
#include <stdexcept>
#include <iostream>
using namespace mkwvk;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
MkwVkCameraObservation primary(uint64_t sequence=1) {
    MkwVkCameraObservation p{};p.size=sizeof p;p.flags=MKWVK_CAMERA_PRIMARY;p.camera_id=100;p.sequence=sequence;p.game_present=20;
    p.view[0]=p.view[5]=p.view[10]=1;p.eye[0]=2;p.view[3]=-2;return p;
}
MkwVkCameraObservation uniforms(uint64_t sequence=2) {
    auto p=primary(sequence);p.flags=MKWVK_CAMERA_UNIFORM;p.camera_id=200;
    p.projection[0]=1;p.projection[5]=1.7f;p.projection[10]=-1.01f;p.projection[11]=-0.2f;p.projection[14]=-1;return p;
}
extern "C" void vr_camera_c_fixture(void (*)(const MkwVkCameraObservation*),size_t sizes[3]);
MkwVkCameraObservation c_sample{};
void receive_c(const MkwVkCameraObservation* sample){c_sample=*sample;}
int main(){try {
    size_t sizes[3]{};vr_camera_c_fixture(receive_c,sizes);
    check(sizes[0]==sizeof(MkwVkCameraObservation)&&sizes[1]==offsetof(MkwVkCameraObservation,view)&&sizes[2]==offsetof(MkwVkCameraObservation,projection),"C/C++ ABI layout");
    check(c_sample.camera_id==123&&c_sample.sequence==456&&c_sample.eye[0]==1.25&&c_sample.view[3]==-1.25f,"C producer/C++ receiver payload");
    CameraCapture capture;CameraCapturePair pair{};
    check(capture.observe(primary(),1000000)&&!capture.latest(1000000,pair),"view alone is no pair");
    check(capture.observe(uniforms(),2000000)&&capture.latest(2000000,pair),"separate object IDs can correlate by matrix");
    check(pair.primary.camera_id==100&&pair.uniforms.camera_id==200&&pair.revision==1,"immutable provenance");
    check(!capture.latest(103000000,pair)&&!pair.revision,"old pairs expire");
    auto changed=primary(3);changed.view[3]=-5;check(capture.observe(changed,104000000),"new camera observation");
    check(!capture.latest(104000000,pair),"new primary invalidates earlier correlation");
    auto u=uniforms(4);check(capture.observe(u,105000000)&&!capture.latest(105000000,pair),"unmatched matrix not accepted");
    u=uniforms(5);u.view[3]=-5;u.game_present=21;
    check(capture.observe(u,106000000)&&!capture.latest(106000000,pair),"different presentation count not accepted");
    u=uniforms(6);u.view[3]=-5;
    check(capture.observe(u,107000000)&&capture.latest(107000000,pair),"fresh matching candidate");
    check(!capture.observe(u,108000000),"reordered or repeated sequence rejected");
    u=uniforms(7);u.projection[0]=std::numeric_limits<float>::quiet_NaN();check(!capture.observe(u,108000000),"invalid projection rejected");
    auto p=primary(8);p.view[0]=2;check(!capture.observe(p,109000000),"non-rigid view rejected");
    p=primary(9);p.flags|=MKWVK_CAMERA_DEBUG;check(!capture.observe(p,110000000)&&!capture.latest(110000000,pair),"debug camera excluded and old candidate cleared");
    p=primary(10);p.size=0;check(!capture.observe(p,111000000),"ABI size checked");
    check(capture.stats().matches==2&&capture.stats().rejected==5,"counts reflect observations, not game frames");
    std::cout<<"Read-only camera observation correlation checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}