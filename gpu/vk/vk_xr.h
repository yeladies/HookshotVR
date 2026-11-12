// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <mutex>

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
// Explicit zero-layer diagnostic; it never submits game images or application Vulkan queue work.
bool xr_vulkan_session_start(VkInstance instance, VkPhysicalDevice physical, VkDevice device, uint32_t queue_family, std::mutex& queue_mutex, uint32_t duration_ms=10000);
bool xr_vulkan_session_stop();
bool xr_vulkan_latest_frame(XrFrameSnapshot* out);
void xr_vulkan_reset();
}