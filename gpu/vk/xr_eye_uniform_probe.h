// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>
namespace mkwvk {
bool xr_eye_uniform_probe_wants(uint64_t frame);
void xr_eye_uniform_probe_record(VkCommandBuffer command,uint64_t recording_value,const void* source,size_t bytes);
}
