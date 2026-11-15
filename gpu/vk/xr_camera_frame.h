// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../../vr/src/vr_camera.h"
namespace mkwvk {
struct XrFrameSnapshot;
// One immutable located frame. The owner supplies native clipping planes and an
// explicit rig/scale. NVN uniforms keep -1..1 depth and +Y; replay adapts once.
VrCameraStatus xr_camera_frame(const XrFrameSnapshot& sample,const VrCameraRig& rig,
                              double near_z,double far_z,VrCameraFrame& out);
}
