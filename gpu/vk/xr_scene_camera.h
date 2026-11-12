// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_camera_capture.h"
#include <cstddef>
namespace mkwvk {
struct SceneCameraEvidence {
    uint64_t replay_frame=0,program=0,color=0,depth=0,uniform_address=0;
    uint32_t width=0,height=0;
};
enum class SceneCameraMatch { Invalid, NoCamera, ViewMismatch, ProjectionMismatch, Match };
// Submitted vertex c3 layout already used by temporal_note_scene_draw. No guest writes.
inline SceneCameraMatch match_camera_uniform(const float* c,size_t count,uint32_t width,uint32_t height,
                                             const CameraCapturePair* pair) {
    if(!c||count<0x230/4||!width||!height)
        return SceneCameraMatch::Invalid;
    for(size_t i=0;i<0x230/4;++i)if(!std::isfinite(c[i]))return SceneCameraMatch::Invalid;
    if(c[0x120/4]!=float(width)||c[0x124/4]!=float(height)||!camera_view_valid(c)||
       !camera_projection_shape(c+0x70/4))return SceneCameraMatch::Invalid;
    // Prove the copied block is internally coherent: VP = projection * affine view.
    for(int r=0;r<4;++r)for(int col=0;col<4;++col) {
        double expected=0;for(int k=0;k<4;++k)expected+=double(c[0x70/4+4*r+k])*(k==3?(col==3?1:0):c[4*k+col]);
        if(std::abs(expected-c[0x30/4+4*r+col])>0.02+std::abs(expected)*0.0001)return SceneCameraMatch::Invalid;
    }
    if(!pair||!pair->revision)return SceneCameraMatch::NoCamera;
    if(!camera_matrix_matches(c,pair->primary.view))return SceneCameraMatch::ViewMismatch;
    for(int i=0;i<16;++i) {
        // Host observation precedes the existing subpixel jitter at P02/P12.
        const double tolerance=(i==2||i==6)?0.01:0.0001;
        if(std::abs(double(c[0x70/4+i])-pair->uniforms.projection[i])>tolerance)return SceneCameraMatch::ProjectionMismatch;
    }
    return SceneCameraMatch::Match;
}
// A uniform match alone is not evidence of a rendered scene attachment.
inline SceneCameraMatch match_scene_camera(const float* c,size_t count,const SceneCameraEvidence& draw,
                                           const CameraCapturePair* pair) {
    if(!draw.color||!draw.depth||!draw.uniform_address||!draw.program)return SceneCameraMatch::Invalid;
    return match_camera_uniform(c,count,draw.width,draw.height,pair);
}
bool xr_scene_capture_enabled();
bool xr_scene_camera_observation(const SceneCameraEvidence&,const float*,size_t);
}
