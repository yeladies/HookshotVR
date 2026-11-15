// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
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
    VrCameraStatus project(const XrFrameSnapshot& frame,double near_z,double far_z,VrCameraFrame& out)const{
        if(!actor_||frame.origin_generation!=origin_)return VR_CAMERA_BAD_IDENTITY;
        VrCameraSample sample{};sample.sequence=frame.sequence;sample.origin_generation=frame.origin_generation;
        sample.predicted_display_time=frame.predicted_display_time;sample.head=pose(frame.head);
        sample.near_z=near_z;sample.far_z=far_z;sample.clip=VR_CAMERA_NVN_MINUS_ONE_TO_ONE;sample.depth=VR_CAMERA_DEPTH_FORWARD;
        for(unsigned eye=0;eye<2;++eye){sample.eyes[eye].pose=pose(frame.eyes[eye].pose);
            for(unsigned k=0;k<4;++k)sample.eyes[eye].fov[k]=frame.eyes[eye].fov[k];}
        return vr_camera_build(&world_rig_,&sample,&out);
    }
};
struct FirstPersonProjectionPacket {
    VrCameraFrame camera{};
    // Render matrices are in game-world units; layer poses stay in the original
    // OpenXR tracking space/metres from this exact immutable observation.
    XrFrameSnapshot tracking{};
    int64_t game_present=0,observed_ns=0;
    bool synthetic=false;
};
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
    bool project(int64_t game_present,double near_z,double far_z,FirstPersonProjectionPacket& out)const{
        // CPU pose retention is bounded, independent of GPU resource leases.
        // No timestamp is refreshed when the renderer consumes an older present.
        for(unsigned k=0;k<count_;++k){const auto& entry=history_[(next_+capacity-1-k)%capacity];
            if(entry.key.game_present!=game_present)continue;
            FirstPersonProjectionPacket next{};
            if(entry.rig.project(entry.frame,near_z,far_z,next.camera)!=VR_CAMERA_OK)return false;
            next.tracking=entry.frame;
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
int xr_first_person_frame(const MkwVkFirstPersonInput*,MkwVkFirstPersonFrame*);
}
