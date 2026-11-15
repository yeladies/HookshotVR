/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HOOKSHOT_VR_CAMERA_H
#define HOOKSHOT_VR_CAMERA_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Pure C, no game memory, runtime calls, clock, filtering or retained camera state.
   RH, +Y up, -Z forward. Quaternions x/y/z/w, rotations local-to-parent.
   Matrices row-major; column vectors: clip = projection * view * world.
   All world translations and near/far share game units. Tracking uses metres. */
enum { VR_CAMERA_POSITION_VALID=1, VR_CAMERA_ORIENTATION_VALID=2 };
typedef enum VrCameraStatus {
    VR_CAMERA_OK=0, VR_CAMERA_BAD_ARGUMENT, VR_CAMERA_BAD_IDENTITY,
    VR_CAMERA_BAD_RIG, VR_CAMERA_BAD_POSE, VR_CAMERA_BAD_PROJECTION
} VrCameraStatus;
typedef enum VrCameraClip { VR_CAMERA_NVN_MINUS_ONE_TO_ONE=0, VR_CAMERA_VULKAN_ZERO_TO_ONE=1 } VrCameraClip;
typedef enum VrCameraDepth { VR_CAMERA_DEPTH_FORWARD=0, VR_CAMERA_DEPTH_REVERSED=1 } VrCameraDepth;
typedef struct VrCameraPose { double orientation[4],position[3]; uint32_t flags; } VrCameraPose;
typedef struct VrCameraView { VrCameraPose pose; double fov[4]; /* left/right/up/down, radians */ } VrCameraView;
typedef struct VrCameraRig {
    double position_world[3],yaw_radians,tracking_reference_m[3],world_units_per_metre;
    uint64_t origin_generation;
} VrCameraRig;
typedef struct VrCameraSample {
    uint64_t sequence,origin_generation;
    int64_t predicted_display_time; /* opaque XR time, never compared with CPU nanoseconds */
    VrCameraPose head;
    VrCameraView eyes[2]; /* absolute located views; eye offset already included */
    double near_z,far_z; /* finite positive near; finite far > near, or +infinity */
    VrCameraClip clip;
    VrCameraDepth depth;
    uint32_t flip_y; /* set only if this projection owns the Y conversion */
} VrCameraSample;
typedef struct VrCameraEye {
    double world_from_eye[16],view[16],projection[16],world_to_clip[16];
    double head_from_eye[16]; /* tracking metres, diagnostic hierarchy; do not apply again */
    double fov[4],planes[6][4]; /* left/right/bottom/top/near/far, world space */
    uint32_t plane_enabled[6];
} VrCameraEye;
typedef struct VrCameraFrame {
    uint64_t sequence,origin_generation;
    int64_t predicted_display_time;
    double world_units_per_metre,near_z,far_z,world_from_head[16];
    VrCameraEye eyes[2];
} VrCameraFrame;
/* Atomic output: failure leaves *out untouched. Rebuild from absolute poses each call. */
VrCameraStatus vr_camera_build(const VrCameraRig* rig,const VrCameraSample* sample,VrCameraFrame* out);
/* Conservative stereo-union culling. Invalid bounds/frame remain visible. */
int vr_camera_bounds_visible(const VrCameraFrame* frame,const double minimum[3],const double maximum[3]);
#ifdef __cplusplus
}
#endif
#endif
