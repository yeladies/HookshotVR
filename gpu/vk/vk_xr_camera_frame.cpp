// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include "xr_camera_frame.h"
namespace mkwvk {
namespace {
VrCameraPose convert(const XrTrackedPose& in){
    VrCameraPose out{};
    for(unsigned k=0;k<4;++k)out.orientation[k]=in.orientation[k];
    for(unsigned k=0;k<3;++k)out.position[k]=in.position[k];
    // OpenXR location flags: orientation valid=1, position valid=2.
    if(in.flags&1)out.flags|=VR_CAMERA_ORIENTATION_VALID;
    if(in.flags&2)out.flags|=VR_CAMERA_POSITION_VALID;
    return out;
}
}
VrCameraStatus xr_camera_frame(const XrFrameSnapshot& in,const VrCameraRig& rig,
                              double near_z,double far_z,VrCameraFrame& out){
    if(!in.running||!in.focused||!in.should_render||in.observed_at_monotonic_ns<=0)return VR_CAMERA_BAD_IDENTITY;
    VrCameraSample sample{};sample.sequence=in.sequence;sample.origin_generation=in.origin_generation;
    sample.predicted_display_time=in.predicted_display_time;sample.head=convert(in.head);
    for(unsigned i=0;i<2;++i){sample.eyes[i].pose=convert(in.eyes[i].pose);
        for(unsigned k=0;k<4;++k)sample.eyes[i].fov[k]=in.eyes[i].fov[k];}
    sample.near_z=near_z;sample.far_z=far_z;sample.clip=VR_CAMERA_NVN_MINUS_ONE_TO_ONE;
    sample.depth=VR_CAMERA_DEPTH_FORWARD;sample.flip_y=0;
    return vr_camera_build(&rig,&sample,&out);
}
}
