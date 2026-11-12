// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>

namespace mkwvk {
// Negotiation only: no session, swapchain, frame submission or game pose override.
bool xr_vulkan_prepare(bool requested);
bool xr_vulkan_active();
bool xr_vulkan_instance(PFN_vkGetInstanceProcAddr get, const VkInstanceCreateInfo& info, VkInstance* out);
bool xr_vulkan_physical(VkInstance instance, VkPhysicalDevice* out);
bool xr_vulkan_device(PFN_vkGetInstanceProcAddr get, VkPhysicalDevice physical, const VkDeviceCreateInfo& info, VkDevice* out);
void xr_vulkan_reset();
}