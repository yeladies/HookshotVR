// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include "xr_immersive_camera.h"
#include <mutex>
#include <memory>
#include <vector>

namespace mkwvk {
// Internal C++ mailbox, not the versioned host ABI. Samples are observations, not game overrides.
struct XrTrackedPose { float orientation[4]{}; float position[3]{}; uint64_t flags=0; };
struct XrTrackedView { XrTrackedPose pose; float fov[4]{}; };
struct XrFrameSnapshot {
    uint64_t sequence=0, origin_generation=0;
    int64_t predicted_display_time=0, observed_at_monotonic_ns=0;
    bool running=false, focused=false, should_render=false;
    XrTrackedPose head;
    XrTrackedView eyes[2];
};
bool xr_vulkan_prepare(bool requested);
bool xr_vulkan_active();
bool xr_vulkan_instance(PFN_vkGetInstanceProcAddr get, const VkInstanceCreateInfo& info, VkInstance* out);
bool xr_vulkan_physical(VkInstance instance, VkPhysicalDevice* out);
bool xr_vulkan_device(PFN_vkGetInstanceProcAddr get, VkPhysicalDevice physical, const VkDeviceCreateInfo& info, VkDevice* out);
// Zero-layer diagnostic by default; visual_preview adds a flat game quad, never native game stereo.
bool xr_vulkan_session_start(VkInstance instance, VkPhysicalDevice physical, VkDevice device, uint32_t queue_family, std::mutex& queue_mutex, uint32_t duration_ms=10000, bool visual_preview=false);
bool xr_vulkan_session_stop();
bool xr_vulkan_latest_frame(XrFrameSnapshot* out);
bool xr_preview_wants_frame();
int xr_immersive_head_camera(uint64_t camera,float rows[12],int64_t present=0);
int xr_immersive_head_lens(float* vertical,float* aspect);
void xr_immersive_note_camera(uint64_t frame,const float* block,size_t count);
void xr_immersive_note_scene_candidate(uint64_t frame);
void xr_immersive_tag_present(int64_t present,uint64_t frame);
ImmersiveTag xr_immersive_frame_tag(int64_t present);
void xr_preview_publish(std::shared_ptr<const std::vector<uint8_t>> pixels,uint32_t width,uint32_t height,int64_t present);
void xr_vulkan_reset();
}