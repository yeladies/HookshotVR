// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_depth_pair_parameters.h"
#include "xr_immersive_camera.h"
namespace hookshot::vr {
struct DepthPairLocatedEye {
    std::array<float,4> orientation{0,0,0,1};std::array<float,3> position{};
    std::array<float,4> fov{};bool orientation_valid=false,position_valid=false;
};
struct DepthPairTrackingTarget {
    std::array<DepthPairLocatedEye,2> eyes;uint64_t origin=0,serial=0;int64_t observed_ns=0;
};
namespace depth_tracking_detail {
inline bool rotation(const float* q,std::array<double,9>& r){
    double n=0;for(unsigned i=0;i<4;++i){if(!std::isfinite(q[i]))return false;n+=double(q[i])*q[i];}if(std::abs(n-1)>.01)return false;
    const double inv=1/std::sqrt(n),x=q[0]*inv,y=q[1]*inv,z=q[2]*inv,w=q[3]*inv;
    r={1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)};return true;
}
}
// Pose metadata only. A source lease/fence owner must independently establish
// immutable GPU storage, device/queue identity and lifetime through submission.
// Coordinates match the existing mono ImmersiveCamera: RH, -Z forward, one
// provisional source unit per metre. The anchor/base game camera cancels.
inline bool prepare_tracked_depth_pair_parameters(const DepthPairSource& source,const mkwvk::ImmersiveTag& captured,
        const DepthPairTrackingTarget& target,int64_t now,uint32_t width,uint32_t height,std::array<DepthPairParameters,2>& out){
    if(now<=0||!source.state||!source.generation||!captured.valid||captured.menu||captured.temporal_stereo||!captured.generation||
       source.origin!=captured.generation||captured.observed_ns<=0||source.capture_pose!=uint64_t(captured.observed_ns)||
       now<captured.observed_ns||now-captured.observed_ns>250000000||target.origin!=captured.generation||!target.serial||
       target.observed_ns<captured.observed_ns||now<target.observed_ns||now-target.observed_ns>50000000)return false;
    std::array<double,9> capture_rotation;if(!depth_tracking_detail::rotation(captured.orientation,capture_rotation))return false;
    for(float x:captured.position)if(!std::isfinite(x))return false;
    std::array<DepthPairParameters,2> next{};
    for(unsigned eye=0;eye<2;++eye){const auto& located=target.eyes[eye];auto& p=next[eye];std::array<double,9> rotation;
        if(!located.orientation_valid||!located.position_valid||!depth_tracking_detail::rotation(located.orientation.data(),rotation))return false;
        double distance=0;for(unsigned k=0;k<3;++k){if(!std::isfinite(located.position[k]))return false;const double d=double(located.position[k])-captured.position[k];distance+=d*d;}if(distance>1)return false;
        for(unsigned r=0;r<3;++r){for(unsigned c=0;c<3;++c){double value=0;for(unsigned k=0;k<3;++k)value+=capture_rotation[3*k+r]*rotation[3*k+c];p.source_from_eye[4*r+c]=float(value);}
            double translation=0;for(unsigned k=0;k<3;++k)translation+=capture_rotation[3*k+r]*(double(located.position[k])-captured.position[k]);p.source_from_eye[4*r+3]=float(translation);}
        for(unsigned i=0;i<4;++i){const float angle=located.fov[i];if(!std::isfinite(angle)||std::abs(angle)>=1.5f)return false;p.eye_tangents[i]=std::tan(angle);}
        p.projection=source.projection;p.optical={source.optical_center[0],source.optical_center[1],25000,.85f};p.dimensions={source.width,source.height,width,height};p.settings={source.format,8,0,0};
        if(!depth_pair_parameters_valid(p))return false;
    }
    out=next;return true;
}
}
