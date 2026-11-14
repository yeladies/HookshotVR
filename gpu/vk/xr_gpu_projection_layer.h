// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_depth_pair_tracking.h"
#include <openxr/openxr.h>
namespace hookshot::vr {
// Metadata from one immutable, completed GPU pair, never separate eye presents.
// GPU allocation/lease ownership is established by the producer/copy owner.
struct GpuProjectionPair {
    uint64_t source_state=0,device_generation=0;uint32_t width=0,height=0;
    int64_t source_observed_ns=0,ready_ns=0;bool immutable=false,complete=false,temporal_source=false;
    DepthPairTrackingTarget target;
};
struct GpuProjectionSwapchain {
    XrSwapchain chain=XR_NULL_HANDLE;uint64_t device_generation=0;
    uint32_t width=0,height=0,array_size=0;bool acquired_and_waited=false,pair_copy_completed=false;
};
// Keep this object alive through xrEndFrame. Copies are safe: layer() binds the
// returned view pointer to this object's own array, not a temporary builder.
struct GpuProjectionData {
    XrSpace space=XR_NULL_HANDLE;
    std::array<XrCompositionLayerProjectionView,2> views{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
    XrCompositionLayerProjection layer()const{XrCompositionLayerProjection value{XR_TYPE_COMPOSITION_LAYER_PROJECTION};value.space=space;value.viewCount=2;value.views=views.data();return value;}
};
inline bool gpu_projection_pair_valid(const GpuProjectionPair& pair,uint64_t origin,int64_t now){
    const auto& target=pair.target;
    if(!origin||now<=0||!pair.source_state||!pair.device_generation||!pair.immutable||!pair.complete||pair.temporal_source||!target.serial||target.origin!=origin||
       pair.source_observed_ns<=0||target.observed_ns<pair.source_observed_ns||pair.ready_ns<target.observed_ns||now<pair.ready_ns||now-pair.source_observed_ns>250000000||
       pair.width<2||pair.height<2||pair.width>4096||pair.height>4096||uint64_t(pair.width)*pair.height>8388608)return false;
    for(const auto& eye:target.eyes){if(!eye.orientation_valid||!eye.position_valid)return false;double norm=0;for(float q:eye.orientation){if(!std::isfinite(q))return false;norm+=double(q)*q;}if(std::abs(norm-1)>.01)return false;
        for(float p:eye.position)if(!std::isfinite(p))return false;for(float angle:eye.fov)if(!std::isfinite(angle)||std::abs(angle)>=1.5f)return false;
        if(eye.fov[0]>=-.0001f||eye.fov[1]<=.0001f||eye.fov[2]<=.0001f||eye.fov[3]>=-.0001f)return false;}
    return true;
}
// No OpenXR call is performed here. Projection poses always describe the GPU
// pair's production sample, not fresh poses pasted onto an older image.
inline bool prepare_gpu_projection_layer(const GpuProjectionPair& pair,const GpuProjectionSwapchain& image,
        XrSpace local,uint64_t current_origin,int64_t now,GpuProjectionData& out){
    if(!local||!gpu_projection_pair_valid(pair,current_origin,now)||pair.device_generation!=image.device_generation||!image.chain||
       !image.acquired_and_waited||!image.pair_copy_completed||image.array_size!=1||image.width!=2*pair.width||image.height!=pair.height)return false;
    const auto& target=pair.target;
    GpuProjectionData next;next.space=local;
    for(unsigned eye=0;eye<2;++eye){const auto& pose=target.eyes[eye];auto& view=next.views[eye];
        if(!pose.orientation_valid||!pose.position_valid)return false;double norm=0;for(float q:pose.orientation){if(!std::isfinite(q))return false;norm+=double(q)*q;}if(std::abs(norm-1)>.01)return false;
        for(float p:pose.position)if(!std::isfinite(p))return false;
        for(float angle:pose.fov)if(!std::isfinite(angle)||std::abs(angle)>=1.5f)return false;
        if(pose.fov[0]>=-.0001f||pose.fov[1]<=.0001f||pose.fov[2]<=.0001f||pose.fov[3]>=-.0001f)return false;
        const float inv=float(1/std::sqrt(norm));view.pose.orientation={pose.orientation[0]*inv,pose.orientation[1]*inv,pose.orientation[2]*inv,pose.orientation[3]*inv};
        view.pose.position={pose.position[0],pose.position[1],pose.position[2]};view.fov={pose.fov[0],pose.fov[1],pose.fov[2],pose.fov[3]};
        view.subImage.swapchain=image.chain;view.subImage.imageArrayIndex=0;view.subImage.imageRect.offset={int32_t(eye*pair.width),0};view.subImage.imageRect.extent={int32_t(pair.width),int32_t(pair.height)};
    }
    out=next;return true;
}
}
