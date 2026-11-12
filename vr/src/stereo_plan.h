// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "camera_math.h"
#include "projection_math.h"
#include <array>
namespace hookshot::vr {
struct GameCameraStamp { uint64_t camera_id=0,revision=0;int64_t present=0,received_ns=0; };
struct EyeFrameInput {
    uint64_t sequence=0,origin_generation=0;
    int64_t predicted_display_time=0,observed_ns=0;
    bool should_render=false;
    XrView eyes[2]{{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
    XrSpaceLocationFlags flags[2]{};
};
struct StereoCalibration {
    TrackingOrigin origin;
    uint64_t camera_id=0,origin_generation=0;
    double near_z=0,far_z=0;
    ClipDepth clip=ClipDepth::MinusOneToOne;
    DepthOrder depth=DepthOrder::Forward;
    bool flip_y=false;
};
struct EyeMatrices { CameraView view;ProjectionMatrix projection{},world_to_clip{}; };
struct DiagnosticStereoPlan {
    GameCameraStamp game;
    uint64_t xr_sequence=0,origin_generation=0;
    int64_t predicted_display_time=0;
    std::array<EyeMatrices,2> eyes;
};
// Pure, unsubmitted plan. Close timestamps are not proof of a shared simulation/render tick.
// A caller must supply an explicit calibration and receive-time stamps from the same host clock.
inline bool build_stereo_plan(const GameCameraStamp& game,const EyeFrameInput& frame,
                              const StereoCalibration& calibration,int64_t now_ns,DiagnosticStereoPlan& out) {
    if(!game.camera_id||!game.revision||game.present<0||!frame.sequence||!frame.origin_generation||
       !frame.should_render||frame.predicted_display_time<=0||
       calibration.camera_id!=game.camera_id||calibration.origin_generation!=frame.origin_generation)return false;
    for(auto at:{game.received_ns,frame.observed_ns})if(at<=0||now_ns<at||now_ns-at>100000000)return false;
    if(std::abs(double(game.received_ns-frame.observed_ns))>50000000)return false;
    DiagnosticStereoPlan candidate{};candidate.game=game;candidate.xr_sequence=frame.sequence;
    candidate.origin_generation=frame.origin_generation;candidate.predicted_display_time=frame.predicted_display_time;
    for(int eye=0;eye<2;++eye) {
        const auto& input=frame.eyes[eye];auto& result=candidate.eyes[eye];
        if(!tracked_camera_view(calibration.origin,input.pose,frame.flags[eye],result.view))return false;
        const auto f=input.fov;
        if(!eye_projection({f.angleLeft,f.angleRight,f.angleUp,f.angleDown},calibration.near_z,calibration.far_z,
                           calibration.clip,calibration.depth,calibration.flip_y,result.projection))return false;
        for(int row=0;row<4;++row)for(int col=0;col<4;++col) {
            double value=0;
            for(int k=0;k<4;++k)value+=result.projection[4*row+k]*(k==3?(col==3?1:0):result.view.rows[k][col]);
            if(!std::isfinite(value))return false;
            result.world_to_clip[4*row+col]=value;
        }
    }
    out=candidate;return true; // Neither eye becomes visible to the caller on a partial failure.
}
}