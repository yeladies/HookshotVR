// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_first_person_projection_audit.h"
#include "vk_api.h"
#include "vk_xr.h"
#include "../../vr/src/vr_camera.h"
#include <cmath>
#include <array>
#include <cstring>
#include <limits>
namespace mkwvk {
// Stateful rig owner; absolute per-frame pose math remains in the C99 module.
// Reset changes yaw/reference only. It never cancels initial head pitch or roll.
class FirstPersonRig {
    uint64_t actor_=0,origin_=0;double reference_[3]{},yaw_=0,anchor_yaw_=0,height_=0;int64_t present_=-1;
    bool synthetic_=false;
    VrCameraRig world_rig_{};
    static VrCameraPose pose(const XrTrackedPose& in){VrCameraPose p{};
        for(unsigned k=0;k<4;++k)p.orientation[k]=in.orientation[k];
        for(unsigned k=0;k<3;++k)p.position[k]=in.position[k];
        if(in.flags&1)p.flags|=VR_CAMERA_ORIENTATION_VALID;if(in.flags&2)p.flags|=VR_CAMERA_POSITION_VALID;return p;}
    static bool narrow(double value,float& out){if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return false;out=float(value);return true;}
public:
    bool build(const MkwVkFirstPersonInput& in,const XrFrameSnapshot& frame,bool synthetic,MkwVkFirstPersonFrame& out){
        if(in.size!=sizeof in||out.size!=sizeof out||!in.actor_id||in.game_present<0||
           (in.flags&~MKWVK_FP_SYNTHETIC_INPUT)||bool(in.flags&MKWVK_FP_SYNTHETIC_INPUT)!=synthetic||
           !frame.sequence||!frame.origin_generation||frame.predicted_display_time<=0||frame.observed_at_monotonic_ns<=0||
           !frame.running||!frame.focused||!frame.should_render||!std::isfinite(in.body_yaw)||
           !std::isfinite(in.eye_height)||in.eye_height<=0||!std::isfinite(in.forward_offset)||
           !std::isfinite(in.world_units_per_metre)||in.world_units_per_metre<=0||
           !std::isfinite(in.turn_radians)||std::abs(in.turn_radians)>.5)return false;
        for(double x:in.root_position)if(!std::isfinite(x))return false;
        VrCameraRig identity{};identity.world_units_per_metre=1;VrCameraTransform head_tracking{};
        const auto head=pose(frame.head);
        if(vr_camera_build_pose(&identity,&head,&head_tracking)!=VR_CAMERA_OK)return false;
        auto next=*this;
        const bool reset=actor_!=in.actor_id||origin_!=frame.origin_generation||height_!=in.eye_height||synthetic_!=synthetic;
        if(reset){
            const double* m=head_tracking.world_from_eye;
            const double yaw=std::hypot(m[2],m[10])>1e-5?std::atan2(m[2],m[10]):std::atan2(-m[8],m[0]);
            next.anchor_yaw_=in.body_yaw+3.14159265358979323846;
            next.yaw_=next.anchor_yaw_-yaw;
            for(unsigned k=0;k<3;++k)next.reference_[k]=head.position[k];
            next.actor_=in.actor_id;next.origin_=frame.origin_generation;next.height_=in.eye_height;next.synthetic_=synthetic;next.present_=-1;
        }
        if(next.present_!=in.game_present){
            next.yaw_=std::remainder(next.yaw_+in.turn_radians,6.28318530717958647692);
            next.anchor_yaw_=std::remainder(next.anchor_yaw_+in.turn_radians,6.28318530717958647692);
        }
        next.present_=in.game_present;
        VrCameraRig rig{};rig.world_units_per_metre=in.world_units_per_metre;rig.origin_generation=frame.origin_generation;rig.yaw_radians=next.yaw_;
        for(unsigned k=0;k<3;++k){rig.position_world[k]=in.root_position[k];rig.tracking_reference_m[k]=next.reference_[k];}
        rig.position_world[0]-=std::sin(next.anchor_yaw_)*in.forward_offset;
        rig.position_world[1]+=in.eye_height;
        rig.position_world[2]-=std::cos(next.anchor_yaw_)*in.forward_offset;
        VrCameraTransform center{};if(vr_camera_build_pose(&rig,&head,&center)!=VR_CAMERA_OK)return false;
        MkwVkFirstPersonFrame result{};result.size=sizeof result;
        result.flags=MKWVK_FP_POSE_VALID|(synthetic?MKWVK_FP_SYNTHETIC_OUTPUT:0)|(reset?MKWVK_FP_RIG_RESET:0);
        result.tracking_sequence=frame.sequence;result.origin_generation=frame.origin_generation;
        result.predicted_display_time=frame.predicted_display_time;result.observed_ns=frame.observed_at_monotonic_ns;
        for(unsigned r=0;r<3;++r){
            result.eye_position[r]=center.world_from_eye[4*r+3];
            result.look_target[r]=result.eye_position[r]-center.view[8+r]*in.world_units_per_metre;
            for(unsigned c=0;c<4;++c)if(!narrow(center.view[4*r+c],result.head_view[4*r+c]))return false;
            if(!narrow(result.eye_position[r],result.eye_target_up[r])||!narrow(result.look_target[r],result.eye_target_up[3+r])||
               !narrow(center.view[4+r],result.eye_target_up[6+r]))return false;
        }
        for(unsigned eye=0;eye<2;++eye){VrCameraTransform e{};const auto located=pose(frame.eyes[eye].pose);
            if(vr_camera_build_pose(&rig,&located,&e)!=VR_CAMERA_OK)return false;
            for(unsigned k=0;k<12;++k)if(!narrow(e.view[k],result.eye_views[eye][k]))return false;
            for(unsigned k=0;k<4;++k){const float f=frame.eyes[eye].fov[k];if(!std::isfinite(f)||std::abs(f)>=1.5707963267948966)return false;result.eye_fov[eye][k]=f;}
            if(result.eye_fov[eye][1]<=result.eye_fov[eye][0]||result.eye_fov[eye][2]<=result.eye_fov[eye][3])return false;
        }
        next.world_rig_=rig;*this=next;out=result;return true;
    }
    VrCameraStatus project(const XrFrameSnapshot& frame,double near_z,double far_z,VrCameraFrame& out,const double* native_projection=nullptr)const{
        if(!actor_||frame.origin_generation!=origin_)return VR_CAMERA_BAD_IDENTITY;
        VrCameraSample sample{};sample.sequence=frame.sequence;sample.origin_generation=frame.origin_generation;
        sample.predicted_display_time=frame.predicted_display_time;sample.head=pose(frame.head);
        sample.near_z=near_z;sample.far_z=far_z;sample.clip=VR_CAMERA_NVN_MINUS_ONE_TO_ONE;sample.depth=VR_CAMERA_DEPTH_FORWARD;
        for(unsigned eye=0;eye<2;++eye){sample.eyes[eye].pose=pose(frame.eyes[eye].pose);
            for(unsigned k=0;k<4;++k)sample.eyes[eye].fov[k]=frame.eyes[eye].fov[k];}
        return native_projection?vr_camera_build_native_projection(&world_rig_,&sample,native_projection,&out):vr_camera_build(&world_rig_,&sample,&out);
    }
};
struct FirstPersonProjectionPacket {
    VrCameraFrame camera{};
    // Render matrices are in game-world units; layer poses stay in the original
    // OpenXR tracking space/metres from this exact immutable observation.
    XrFrameSnapshot tracking{};
    int64_t game_present=0,observed_ns=0;
    bool synthetic=false,native_depth_preserved=false;
};
inline FirstPersonProjectionEvidence first_person_projection_evidence(const FirstPersonProjectionPacket& packet,
                                                                      const float view[12],const float projection[16]){
    FirstPersonProjectionEvidence out{};
    if(!view||!projection||!packet.native_depth_preserved)return out;
    const auto& f=packet.camera;const auto& tracking=packet.tracking;
    if(!f.sequence||f.sequence!=tracking.sequence||f.origin_generation!=tracking.origin_generation||
       f.predicted_display_time!=tracking.predicted_display_time||packet.observed_ns!=tracking.observed_at_monotonic_ns)return out;
    out.available=true;out.center_matches=out.depth_exact=out.fov_matches=true;out.synthetic=packet.synthetic;
    out.sequence=f.sequence;out.origin=f.origin_generation;out.predicted_time=f.predicted_display_time;out.observed_ns=packet.observed_ns;
    out.effective_near=f.near_z;out.effective_far=f.far_z;
    const auto* world=f.world_from_head;
    for(unsigned r=0;r<3;++r){
        for(unsigned c=0;c<3;++c)if(!std::isfinite(view[4*r+c])||std::abs(world[4*c+r]-view[4*r+c])>.0001)out.center_matches=false;
        double t=0;for(unsigned k=0;k<3;++k)t-=world[4*k+r]*world[4*k+3];
        if(!std::isfinite(view[4*r+3])||std::abs(t-view[4*r+3])>.01)out.center_matches=false;
    }
    double separation2=0;
    for(unsigned k=0;k<3;++k){double d=f.eyes[0].world_from_eye[4*k+3]-f.eyes[1].world_from_eye[4*k+3];separation2+=d*d;}
    out.eye_separation_world=std::sqrt(separation2);
    for(unsigned eye=0;eye<2;++eye){
        const auto* p=f.eyes[eye].projection;const auto* angles=tracking.eyes[eye].fov;
        if(p[10]!=double(projection[10])||p[11]!=double(projection[11])||p[14]!=-1||p[15]!=0)out.depth_exact=false;
        const double l=std::tan(double(angles[0])),r=std::tan(double(angles[1])),u=std::tan(double(angles[2])),d=std::tan(double(angles[3]));
        const double expected[]={2/(r-l),(r+l)/(r-l),2/(u-d),(u+d)/(u-d)};
        const double actual[]={p[0],p[2],p[5],p[6]};
        for(unsigned k=0;k<4;++k)if(!std::isfinite(expected[k])||!std::isfinite(actual[k])||std::abs(actual[k]-expected[k])>1e-10)out.fov_matches=false;
    }
    return out;
}
// One game present owns one immutable camera packet, even when the engine
// asks for its view repeatedly or tracking advances on the XR worker.
class FirstPersonFrameOwner {
    struct Entry {
        FirstPersonRig rig;
        MkwVkFirstPersonInput key{};
        MkwVkFirstPersonFrame packet{};
        XrFrameSnapshot frame{};
    };
    static constexpr unsigned capacity=64;
    std::array<Entry,capacity> history_{};
    unsigned next_=0,count_=0;
    FirstPersonRig rig_;
    MkwVkFirstPersonInput key_{};
    MkwVkFirstPersonFrame packet_{};
    static bool same_config(const MkwVkFirstPersonInput& a,const MkwVkFirstPersonInput& b){
        return a.actor_id==b.actor_id&&a.flags==b.flags&&a.eye_height==b.eye_height&&
               a.forward_offset==b.forward_offset&&a.world_units_per_metre==b.world_units_per_metre;
    }
public:
    bool build(const MkwVkFirstPersonInput& in,const XrFrameSnapshot& frame,bool synthetic,MkwVkFirstPersonFrame& out){
        if(out.size!=sizeof out||(packet_.size&&in.game_present<key_.game_present))return false;
        auto next=rig_;MkwVkFirstPersonFrame packet{};packet.size=sizeof packet;
        if(!next.build(in,frame,synthetic,packet))return false;
        if(packet_.size&&key_.game_present==in.game_present&&same_config(key_,in)&&
           packet_.origin_generation==frame.origin_generation){out=packet_;return true;}
        if((packet.flags&MKWVK_FP_RIG_RESET)||(packet_.size&&!same_config(key_,in))){count_=0;next_=0;}
        rig_=next;key_=in;packet_=packet;
        auto& entry=history_[next_];entry.rig=next;entry.key=in;entry.packet=packet;entry.frame=frame;
        next_=(next_+1)%capacity;if(count_<capacity)++count_;
        out=packet;return true;
    }
    bool project(int64_t game_present,double near_z,double far_z,FirstPersonProjectionPacket& out,const double* native_projection=nullptr,FirstPersonProjectionLookup* lookup=nullptr)const{
        if(lookup){
            *lookup={};lookup->status=1;lookup->retained=count_;
            if(count_){lookup->newest_present=history_[(next_+capacity-1)%capacity].key.game_present;
                lookup->oldest_present=history_[(next_+capacity-count_)%capacity].key.game_present;}
        }
        // CPU pose retention is bounded, independent of GPU resource leases.
        // No timestamp is refreshed when the renderer consumes an older present.
        for(unsigned k=0;k<count_;++k){const auto& entry=history_[(next_+capacity-1-k)%capacity];
            if(entry.key.game_present!=game_present)continue;
            FirstPersonProjectionPacket next{};
            const auto status=entry.rig.project(entry.frame,near_z,far_z,next.camera,native_projection);
            if(lookup){lookup->status=status==VR_CAMERA_OK?3:2;lookup->camera_status=int(status);}
            if(status!=VR_CAMERA_OK)return false;
            next.tracking=entry.frame;next.native_depth_preserved=native_projection!=nullptr;
            next.game_present=game_present;next.observed_ns=entry.packet.observed_ns;
            next.synthetic=(entry.packet.flags&MKWVK_FP_SYNTHETIC_OUTPUT)!=0;
            out=next;return true;
        }
        return false;
    }
};
// Consumer supplies authoritative native near/far. This retrieves a frozen
// packet; it cannot advance tracking or relabel the observation timestamp.
bool xr_first_person_projection(int64_t game_present,double near_z,double far_z,FirstPersonProjectionPacket* out);
// Native depth coefficients are retained exactly; runtime FOVs own XY.
bool xr_first_person_native_projection(int64_t game_present,const float native_projection[16],FirstPersonProjectionPacket* out,FirstPersonProjectionLookup* lookup=nullptr);
int xr_first_person_frame(const MkwVkFirstPersonInput*,MkwVkFirstPersonFrame*);
}
