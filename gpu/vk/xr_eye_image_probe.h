// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace mkwvk {
struct Texture;
bool xr_eye_image_probe_wants(uint64_t frame);
void xr_eye_image_probe_record(VkCommandBuffer command,const Texture* color,const Texture* depth,uint64_t recording_value);
}
