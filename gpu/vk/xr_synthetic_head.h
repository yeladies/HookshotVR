// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_immersive_camera.h"
namespace mkwvk {
// Diagnostic only: the caller rejects this path whenever an XR session is active.
// Begin centred so reference anchoring cannot cancel the requested test angle.
inline double synthetic_head_angle(const char* mode,double elapsed){
    if(!std::isfinite(elapsed)||elapsed<0)return 0;
    if(mode&&(!std::strcmp(mode,"WideYaw")||!std::strcmp(mode,"WidePitch")))
        return elapsed<=3?0:.7853981633974483*std::sin((elapsed-3)*.3141592653589793);
    return .12*std::sin(elapsed*.7);
}
inline void synthetic_head_pose(const char* mode,double elapsed,ImmersivePose& pose){
    const double angle=synthetic_head_angle(mode,elapsed);
    const unsigned axis=mode&&!std::strcmp(mode,"WidePitch")?0:1;
    pose.q[0]=pose.q[1]=pose.q[2]=0;pose.q[axis]=float(std::sin(angle/2));pose.q[3]=float(std::cos(angle/2));
    pose.p[0]=axis==1&&(!mode||!std::strcmp(mode,"Small"))?float(.04*std::sin(elapsed)):0;
}
}
