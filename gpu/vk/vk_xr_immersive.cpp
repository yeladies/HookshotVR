// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include <chrono>
#include <mutex>
#include <cstdlib>
#include <fstream>
#include <array>
namespace mkwvk {
void logf(const char*,...);
namespace {
bool enabled(){static const bool v=[] {const auto* s=std::getenv("RX_VK_XR_IMMERSIVE");return s&&std::strcmp(s,"1")==0;}();return v;}
bool synthetic(){static const bool v=[] {const auto* s=std::getenv("HOOKSHOT_VR_IMMERSIVE_TEST");return s&&std::strcmp(s,"1")==0;}();return v;}
int64_t now_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct State {
    std::mutex mutex;ImmersiveCamera camera;ImmersiveTag scene;uint64_t scene_frame=UINT64_MAX;
    struct Present {int64_t id=-1;ImmersiveTag tag;};std::array<Present,32> presents{};
    uint64_t applied=0,matched=0,tagged=0,published=0;int64_t epoch=0;
};
State& state(){static auto* p=new State;return *p;}
void report(State& s){
    if(const char* path=std::getenv("HOOKSHOT_VR_IMMERSIVE_REPORT_FILE")){
        std::ofstream out(path);if(out)out<<"{\"schema\":1,\"camera_overrides\":"<<s.applied<<",\"matched_draws\":"<<s.matched<<",\"tagged_presents\":"<<s.tagged<<",\"published_images\":"<<s.published<<",\"synthetic_pose\":"<<(synthetic()?"true":"false")<<",\"monoscopic\":true,\"native_stereo\":false}\n";
    }
}
}
int xr_immersive_head_camera(uint64_t id,float rows[12]){
    if(!enabled()||!rows)return 0;auto& s=state();std::lock_guard lock(s.mutex);const auto now=now_ns();ImmersivePose pose;
    if(synthetic()){
        if(xr_vulkan_active())return 0; // Synthetic motion must never reach a headset session.
        if(!s.epoch)s.epoch=now;const double t=(now-s.epoch)*1e-9;const double angle=.12*std::sin(t*.7);
        pose.q[1]=float(std::sin(angle/2));pose.q[3]=float(std::cos(angle/2));pose.p[0]=float(.04*std::sin(t));pose.generation=1;pose.observed_ns=now;
    }else{
        XrFrameSnapshot frame;if(!xr_vulkan_latest_frame(&frame)||!frame.focused||!frame.should_render||(frame.head.flags&3)!=3)return 0;
        std::memcpy(pose.q,frame.head.orientation,sizeof pose.q);std::memcpy(pose.p,frame.head.position,sizeof pose.p);pose.generation=frame.origin_generation;pose.observed_ns=frame.observed_at_monotonic_ns;
    }
    if(!s.camera.apply(id,rows,pose,now))return 0;++s.applied;
    if(s.applied==1||s.applied%120==0){report(s);logf("vr-immersive: camera overrides %llu, mono, synthetic=%d",(unsigned long long)s.applied,synthetic());}return 1;
}
void xr_immersive_note_camera(uint64_t frame,const float* block,size_t count){
    if(!enabled()||!block||count<0x230/4)return;auto& s=state();std::lock_guard lock(s.mutex);
    auto tag=s.camera.match(block,block+0x70/4,now_ns());if(!tag.valid)return;
    s.scene=tag;s.scene_frame=frame;++s.matched;
}
void xr_immersive_tag_present(int64_t present,uint64_t frame){
    if(!enabled()||present<0)return;auto& s=state();std::lock_guard lock(s.mutex);auto& p=s.presents[uint64_t(present)%s.presents.size()];p={present,{}};
    const auto now=now_ns();if(s.scene_frame==frame&&s.scene.valid&&now>=s.scene.observed_ns&&now-s.scene.observed_ns<250000000){p.tag=s.scene;++s.tagged;}
}
ImmersiveTag xr_immersive_frame_tag(int64_t present){
    if(!enabled()||present<0)return {};auto& s=state();std::lock_guard lock(s.mutex);const auto& p=s.presents[uint64_t(present)%s.presents.size()];
    if(p.id!=present||!p.tag.valid)return {};++s.published;if(s.published==1||s.published%30==0)report(s);return p.tag;
}
}
