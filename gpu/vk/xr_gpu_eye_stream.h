// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include "xr_depth_pair_tracking.h"
#include <memory>
namespace mkwvk {
struct XrGpuEyeFrame {
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;
    uint64_t bytes=0,state=0,source_state=0,generation=0,recording=0,replay_frame=0;
    VkFormat format=VK_FORMAT_R8G8B8A8_UNORM; // raw clamped compute output; final game tone mapping is separate
    uint32_t width=0,height=0,queue_family=0;int64_t source_observed_ns=0,ready_ns=0;
    hookshot::vr::DepthPairTrackingTarget target;bool synthetic=false;
private:
    std::shared_ptr<const void> lifetime_;friend struct XrGpuEyeFrameAccess;
};
bool xr_gpu_eye_stream_enabled();
void xr_gpu_eye_stream_tick(uint64_t frame);
void xr_gpu_eye_stream_network(VkCommandBuffer,uint64_t recording,uint64_t program);
std::shared_ptr<const XrGpuEyeFrame> xr_gpu_eye_stream_acquire(VkDevice,uint64_t origin,int64_t now,bool allow_synthetic=false);
bool xr_gpu_eye_stream_stop();
}
