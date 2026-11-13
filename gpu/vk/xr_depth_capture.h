// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>
namespace mkwvk {
struct Texture;
bool xr_depth_capture_enabled();
void xr_depth_capture_tick(uint64_t frame);
void xr_depth_capture_note(const Texture*,const Texture*,const float* camera,size_t count);
void xr_depth_capture_view(const Texture*,VkFormat);
void xr_depth_capture_network(VkCommandBuffer,uint64_t recording,uint64_t program,const std::vector<const Texture*>&);
}
