// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_first_person_rig.h"
#include <chrono>
#include <cstdio>
#include <stdexcept>
namespace mkwvk {
static bool active=false,available=true;static int64_t timestamp_offset=0;static XrFrameSnapshot sample;
bool xr_vulkan_active(){return active;}
bool xr_vulkan_latest_frame(XrFrameSnapshot* out){
    if(!available)return false;*out=sample;
    // Publication occurs after the callback entered. Reading the clock before
    // this mailbox would reject every fresh packet as being from the future.
    out->observed_at_monotonic_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()+timestamp_offset;
    return true;
}
}
static void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(){try{
    using namespace mkwvk;
    sample.sequence=9;sample.origin_generation=2;sample.predicted_display_time=123;
    sample.running=sample.focused=sample.should_render=true;sample.head.flags=3;sample.head.orientation[3]=1;
    for(unsigned k=0;k<2;++k){auto& e=sample.eyes[k];e.pose=sample.head;e.pose.position[0]=k?.032f:-.032f;
        e.fov[0]=-.7f;e.fov[1]=.8f;e.fov[2]=.9f;e.fov[3]=-.6f;}
    MkwVkFirstPersonInput in{};in.size=sizeof in;in.actor_id=1;in.game_present=1;in.eye_height=1.5;in.world_units_per_metre=1;
    MkwVkFirstPersonFrame out{};out.size=sizeof out;
    check(xr_first_person_frame(&in,&out)==1,"fresh publication after entry is accepted");const auto saved=out;
    FirstPersonProjectionPacket projected{};
    check(xr_first_person_projection(in.game_present,.125,1234,&projected)&&projected.camera.sequence==out.tracking_sequence&&!projected.synthetic,"native consumer uses same camera packet");
    check(!xr_first_person_projection(in.game_present,.125,1234,nullptr),"null projection output rejected");
    timestamp_offset=-1000000000;check(!xr_first_person_frame(&in,&out),"stale pose rejected");
    timestamp_offset=1000000000;check(!xr_first_person_frame(&in,&out),"future observation rejected");timestamp_offset=0;
    available=false;check(!xr_first_person_frame(&in,&out),"missing mailbox rejected");available=true;
    sample.focused=false;check(!xr_first_person_frame(&in,&out),"unfocused session rejected");sample.focused=true;
    check(!std::memcmp(&out,&saved,sizeof out),"rejections preserve complete output");
    in.flags=MKWVK_FP_SYNTHETIC_INPUT;active=true;check(!xr_first_person_frame(&in,&out),"synthetic sample cannot enter an active XR renderer");
    active=false;check(xr_first_person_frame(&in,&out)==1&&(out.flags&MKWVK_FP_SYNTHETIC_OUTPUT),"explicit non-XR synthetic provenance");
    std::puts("First-person callback freshness, session and synthetic gates passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
