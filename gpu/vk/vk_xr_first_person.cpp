// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_first_person_rig.h"
#include <mutex>
#include <chrono>
#include <cstdlib>
namespace mkwvk {
namespace {
struct FirstPersonContext {std::mutex mutex;FirstPersonFrameOwner rig;int64_t epoch=0;};
FirstPersonContext& first_person_context(){static FirstPersonContext context;return context;}
}
bool xr_first_person_projection(int64_t game_present,double near_z,double far_z,FirstPersonProjectionPacket* out){
    if(!out)return false;
    auto& context=first_person_context();std::lock_guard lock(context.mutex);
    return context.rig.project(game_present,near_z,far_z,*out);
}
bool xr_first_person_native_projection(int64_t game_present,const float projection[16],FirstPersonProjectionPacket* out,FirstPersonProjectionLookup* lookup){
    if(!projection||!out)return false;
    double native[16];for(unsigned i=0;i<16;++i)native[i]=projection[i];
    auto& context=first_person_context();std::lock_guard lock(context.mutex);
    return context.rig.project(game_present,0,0,*out,native,lookup);
}
FirstPersonProjectionEvidence xr_first_person_native_projection_evidence(int64_t game_present,const float view[12],const float projection[16]){
    FirstPersonProjectionPacket packet{};FirstPersonProjectionLookup lookup{};
    const bool available=xr_first_person_native_projection(game_present,projection,&packet,&lookup);
    auto evidence=available?first_person_projection_evidence(packet,view,projection):FirstPersonProjectionEvidence{};
    evidence.lookup=lookup;return evidence;
}
int xr_first_person_frame(const MkwVkFirstPersonInput* input,MkwVkFirstPersonFrame* out){
    if(!input||!out||input->size!=sizeof *input||out->size!=sizeof *out)return 0;
    auto& context=first_person_context();auto& mutex=context.mutex;auto& rig=context.rig;auto& epoch=context.epoch;
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const bool synthetic=(input->flags&MKWVK_FP_SYNTHETIC_INPUT)!=0;XrFrameSnapshot frame;
    if(synthetic){
        if(xr_vulkan_active())return 0;
        std::lock_guard lock(mutex);if(!epoch)epoch=now;const double t=(now-epoch)*1e-9;
        const char* sweep=std::getenv("HOOKSHOT_VR_FIRST_PERSON_SWEEP");
        const bool down=sweep&&!std::strcmp(sweep,"LookDown");
        const bool lean=sweep&&!std::strcmp(sweep,"Lean");
        const bool wide_yaw=sweep&&!std::strcmp(sweep,"WideYaw"),wide_pitch=sweep&&!std::strcmp(sweep,"WidePitch");
        const bool pitch=down||wide_pitch||(sweep&&!std::strcmp(sweep,"Pitch"));
        const double angle=down?-1.0471975511965976:(sweep&&std::strcmp(sweep,"None")?(lean?.2:wide_yaw?1.3962634015954636:wide_pitch?1.2:.5)*std::sin(t*.5):0);
        frame.sequence=uint64_t(input->game_present)+1;frame.origin_generation=1;frame.predicted_display_time=now;frame.observed_at_monotonic_ns=now;
        frame.running=frame.focused=frame.should_render=true;frame.head.orientation[3]=float(std::cos(angle*.5));frame.head.orientation[pitch?0:1]=float(std::sin(angle*.5));frame.head.flags=3;
        if(lean){frame.head.position[0]=float(.2*std::sin(t*.7));frame.head.position[1]=float(.1*std::sin(t*.5));frame.head.position[2]=float(.15*std::sin(t*.6));}
        for(unsigned i=0;i<2;++i){auto& e=frame.eyes[i];e.pose=frame.head;const double side=i?.032:-.032;
            e.pose.position[0]+=float(side*(pitch?1:std::cos(angle)));e.pose.position[2]-=float(pitch?0:side*std::sin(angle));
            e.fov[0]=-.7f;e.fov[1]=.8f;e.fov[2]=.9f;e.fov[3]=-.6f;}
        return rig.build(*input,frame,true,*out)?1:0;
    }
    if(!xr_vulkan_latest_frame(&frame))return 0;
    // Read the clock after the mailbox: a concurrent publication can be newer
    // than the clock taken on entry without being a future tracking sample.
    const auto observed_now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(observed_now<frame.observed_at_monotonic_ns||observed_now-frame.observed_at_monotonic_ns>50000000)return 0;
    std::lock_guard lock(mutex);return rig.build(*input,frame,false,*out)?1:0;
}
}
