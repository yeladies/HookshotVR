// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_baseline_recorder.h"
namespace mkwvk {
struct Program;
bool xr_baseline_draw_enabled();
void xr_baseline_draw_tick(uint64_t frame,uint64_t serial);
bool xr_baseline_draw_selected(const Program&);
void xr_baseline_draw_prepare(const XrBaselineRecorderInput&);
void xr_baseline_draw_finish(uint64_t serial,uint64_t recording,uint64_t program,VkCommandBuffer,
                             const std::vector<VkWriteDescriptorSet>&);
}
