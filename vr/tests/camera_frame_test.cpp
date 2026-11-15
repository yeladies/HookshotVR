// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include "xr_camera_frame.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <cstdio>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    mkwvk::XrFrameSnapshot in;in.sequence=33;in.origin_generation=7;in.predicted_display_time=123456;
    in.observed_at_monotonic_ns=8;in.running=in.focused=in.should_render=true;
    in.head.orientation[3]=1;in.head.flags=3;
    for(unsigned i=0;i<2;++i){auto& e=in.eyes[i];e.pose=in.head;e.pose.position[0]=i?.032f:-.032f;
        e.fov[0]=-.7f;e.fov[1]=.8f;e.fov[2]=.9f;e.fov[3]=-.6f;}
    VrCameraRig rig{};rig.origin_generation=7;rig.world_units_per_metre=10;VrCameraFrame frame{};
    check(mkwvk::xr_camera_frame(in,rig,.1,25000,frame)==VR_CAMERA_OK,"build located frame");
    check(frame.sequence==33&&frame.predicted_display_time==123456,"preserve XR frame/time");
    check(std::abs(frame.eyes[1].world_from_eye[3]-frame.eyes[0].world_from_eye[3]-.64)<1e-6,"IPD scaled once");
    check(frame.eyes[0].projection[5]>0&&frame.eyes[0].projection[14]==-1,"NVN +Y and -1..1 contract");
    const auto saved=frame;
    in.eyes[1].pose.flags=1;
    check(mkwvk::xr_camera_frame(in,rig,.1,25000,frame)==VR_CAMERA_BAD_POSE,"orientation-only flags cannot imply position");
    check(!std::memcmp(&frame,&saved,sizeof frame),"failed frame preserves output");
    in.eyes[1].pose.flags=2;check(mkwvk::xr_camera_frame(in,rig,.1,25000,frame)==VR_CAMERA_BAD_POSE,"position-only flags cannot imply orientation");
    in.eyes[1].pose.flags=3;in.origin_generation=8;check(mkwvk::xr_camera_frame(in,rig,.1,25000,frame)==VR_CAMERA_BAD_IDENTITY,"recenter needs a matching rig");
    in.origin_generation=7;in.should_render=false;check(mkwvk::xr_camera_frame(in,rig,.1,25000,frame)==VR_CAMERA_BAD_IDENTITY,"hidden frame rejected");
    check(!std::memcmp(&frame,&saved,sizeof frame),"invalid XR sample does not relabel old matrices");
    std::puts("Native C++ to C camera frame contract passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
