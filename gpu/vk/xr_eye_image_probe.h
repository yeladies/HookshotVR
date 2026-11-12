// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace mkwvk {
struct Texture;
struct EyeProbeView {uint32_t level=0,layer=0;VkFormat format=VK_FORMAT_UNDEFINED;};
bool xr_eye_image_probe_wants(uint64_t frame);
void xr_eye_image_probe_record(VkCommandBuffer command,const Texture* color,const Texture* depth,uint64_t recording_value,const EyeProbeView& color_view,const EyeProbeView& depth_view);
}
