// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace mkwvk {
bool xr_gpu_depth_pair_probe_enabled();
void xr_gpu_depth_pair_probe_tick(uint64_t frame);
void xr_gpu_depth_pair_probe_network(VkCommandBuffer,uint64_t recording,uint64_t program);
bool xr_gpu_depth_pair_probe_stop();
}
