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
bool temporal(){static const bool v=[] {const auto* s=std::getenv("RX_VK_XR_TEMPORAL_STEREO");return s&&std::strcmp(s,"1")==0;}();return v;}
bool synthetic(){static const bool v=[] {const auto* s=std::getenv("HOOKSHOT_VR_IMMERSIVE_TEST");return s&&std::strcmp(s,"1")==0;}();return v;}
int64_t now_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct State {
    std::mutex mutex;ImmersiveCamera camera;ImmersiveTag scene;uint64_t scene_frame=UINT64_MAX,candidate_frame=UINT64_MAX;uint32_t scene_width=0,scene_height=0;
    struct Present {int64_t id=-1;ImmersiveTag tag;};std::array<Present,32> presents{};
    uint64_t applied=0,matched=0,tagged=0,published=0,menu_images=0,eye_images[2]{},lens_updates=0;float horizontal_fov=0,vertical_fov=0,requested_vertical=0,requested_aspect=0;int64_t epoch=0;
};
State& state(){static auto* p=new State;return *p;}
void report(State& s){
    if(const char* path=std::getenv("HOOKSHOT_VR_IMMERSIVE_REPORT_FILE")){
        std::ofstream out(path);if(out)out<<"{\"schema\":1,\"camera_overrides\":"<<s.applied<<",\"matched_draws\":"<<s.matched<<",\"tagged_presents\":"<<s.tagged<<",\"published_images\":"<<s.published<<",\"menu_images\":"<<s.menu_images<<",\"left_images\":"<<s.eye_images[0]<<",\"right_images\":"<<s.eye_images[1]<<",\"lens_updates\":"<<s.lens_updates<<",\"horizontal_fov_rad\":"<<s.horizontal_fov<<",\"vertical_fov_rad\":"<<s.vertical_fov<<",\"requested_vertical_fov_rad\":"<<s.requested_vertical<<",\"requested_aspect\":"<<s.requested_aspect<<",\"synthetic_pose\":"<<(synthetic()?"true":"false")<<",\"monoscopic\":"<<(temporal()?"false":"true")<<",\"temporal_stereo\":"<<(temporal()?"true":"false")<<",\"native_stereo\":false}\n";
    }
}
}
int xr_immersive_head_camera(uint64_t id,float rows[12],int64_t present){
    if(!enabled()||!rows||present<0)return 0;auto& s=state();std::lock_guard lock(s.mutex);const auto now=now_ns();ImmersivePose pose,anchor;
    const bool stereo=[] {auto* e=std::getenv("RX_VK_XR_TEMPORAL_STEREO");return e&&std::strcmp(e,"1")==0;}();
    const unsigned eye=unsigned((uint64_t(present)/2)%2);
    if(synthetic()){
        if(xr_vulkan_active())return 0;
        if(!s.epoch)s.epoch=now;const double t=(now-s.epoch)*1e-9;const double angle=.12*std::sin(t*.7);
        pose.q[1]=float(std::sin(angle/2));pose.q[3]=float(std::cos(angle/2));pose.p[0]=float(.04*std::sin(t));pose.generation=1;pose.observed_ns=now;anchor=pose;
        if(stereo){const double side=eye?.032:-.032;pose.p[0]+=float(side*std::cos(angle));pose.p[2]-=float(side*std::sin(angle));}
    }else{
        XrFrameSnapshot frame;if(!xr_vulkan_latest_frame(&frame)||!frame.focused||!frame.should_render||(frame.head.flags&3)!=3)return 0;
        std::memcpy(anchor.q,frame.head.orientation,sizeof anchor.q);std::memcpy(anchor.p,frame.head.position,sizeof anchor.p);anchor.generation=frame.origin_generation;anchor.observed_ns=frame.observed_at_monotonic_ns;pose=anchor;
        if(stereo){if((frame.eyes[eye].pose.flags&3)!=3)return 0;std::memcpy(pose.q,frame.eyes[eye].pose.orientation,sizeof pose.q);std::memcpy(pose.p,frame.eyes[eye].pose.position,sizeof pose.p);}
    }
    pose.temporal_stereo=stereo;pose.eye=eye;
    if(!s.camera.apply(id,rows,pose,now,stereo?&anchor:nullptr))return 0;++s.applied;
    if(s.applied==1||s.applied%120==0){report(s);logf("vr-immersive: camera overrides %llu, temporal stereo=%d, synthetic=%d",(unsigned long long)s.applied,stereo,synthetic());}return 1;
}
int xr_immersive_head_lens(float* vertical,float* aspect){
    if(!enabled()||!vertical||!aspect)return 0;
    const char* wide=std::getenv("RX_VK_XR_WIDE_FOV");if(!wide||std::strcmp(wide,"1"))return 0;
    float fov=0,ratio=0;
    if(synthetic()){
        if(xr_vulkan_active())return 0;
        constexpr float radians=.017453292519943295f;const float a[4]={-55*radians,55*radians,50*radians,-50*radians};
        if(!immersive_lens_from_fovs(a,a,fov,ratio))return 0;
    }else{
        XrFrameSnapshot frame;if(!xr_vulkan_latest_frame(&frame)||!frame.focused||!frame.should_render||(frame.eyes[0].pose.flags&3)!=3||(frame.eyes[1].pose.flags&3)!=3||!immersive_lens_from_fovs(frame.eyes[0].fov,frame.eyes[1].fov,fov,ratio))return 0;
    }
    {auto& s=state();std::lock_guard lock(s.mutex);++s.lens_updates;s.requested_vertical=fov;s.requested_aspect=ratio;}
    *vertical=fov;*aspect=ratio;return 1;
}
void xr_immersive_note_scene_candidate(uint64_t frame,uint32_t width,uint32_t height){
    if(!enabled())return;auto& s=state();std::lock_guard lock(s.mutex);
    // Preserve fail-closed behavior for a main-aspect perspective, including
    // dynamic resolution. A separately rendered inventory portrait is not the world.
    if(s.scene_width&&s.scene_height&&width&&height){
        const double expected=double(height)*s.scene_width;
        if(std::abs(double(width)*s.scene_height-expected)>expected*.01)return;
    }
    if(synthetic()&&s.scene.valid&&now_ns()-s.scene.observed_ns>250000000){static uint64_t last=UINT64_MAX;if(last==UINT64_MAX||frame-last>=60){last=frame;logf("vr-immersive: blocked stale-world candidate %ux%u, main %ux%u",width,height,s.scene_width,s.scene_height);}}
    s.candidate_frame=frame;
}
void xr_immersive_note_camera(uint64_t frame,const float* block,size_t count){
    if(!enabled()||!block||count<0x230/4)return;auto& s=state();std::lock_guard lock(s.mutex);
    auto tag=s.camera.match(block,block+0x70/4,now_ns());if(!tag.valid)return;
    if(block[0x120/4]>0&&block[0x120/4]<=16384&&block[0x124/4]>0&&block[0x124/4]<=16384){s.scene_width=uint32_t(block[0x120/4]);s.scene_height=uint32_t(block[0x124/4]);}
    s.scene=tag;s.scene_frame=frame;++s.matched;s.horizontal_fov=tag.fov[1]-tag.fov[0];s.vertical_fov=tag.fov[2]-tag.fov[3];
}
void xr_immersive_tag_present(int64_t present,uint64_t frame){
    if(!enabled()||present<0)return;auto& s=state();std::lock_guard lock(s.mutex);auto& p=s.presents[uint64_t(present)%s.presents.size()];p={present,{}};
    const auto now=now_ns();if(s.scene_frame==frame&&s.scene.valid&&now>=s.scene.observed_ns&&now-s.scene.observed_ns<250000000){p.tag=s.scene;++s.tagged;}
    else if(s.candidate_frame!=frame){
        // An explicitly UI-only present may use a 2D layer. A 3D frame with
        // missing camera provenance must still be suppressed, never guessed.
        XrFrameSnapshot current;
        if(xr_vulkan_latest_frame(&current)&&current.focused&&(current.head.flags&3)==3){p.tag.menu=true;p.tag.generation=current.origin_generation;p.tag.observed_ns=now;}
        else if(synthetic()&&!xr_vulkan_active()){p.tag.menu=true;p.tag.generation=1;p.tag.observed_ns=now;}
    }
}
ImmersiveTag xr_immersive_frame_tag(int64_t present){
    if(!enabled()||present<0)return {};auto& s=state();std::lock_guard lock(s.mutex);const auto& p=s.presents[uint64_t(present)%s.presents.size()];
    if(p.id!=present||(!p.tag.valid&&!p.tag.menu))return {};++s.published;if(p.tag.menu)++s.menu_images;else if(p.tag.temporal_stereo&&p.tag.eye<2)++s.eye_images[p.tag.eye];if(s.published==1||s.published%30==0||(p.tag.menu&&s.menu_images%30==1))report(s);return p.tag;
}
}
