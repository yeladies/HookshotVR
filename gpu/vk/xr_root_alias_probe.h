// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_uniform_copy_source.h"
#include <array>
namespace mkwvk {
struct XrRootAliasProbeInput {
    VkCommandBuffer command=VK_NULL_HANDLE;uint64_t recording=0,guest_camera=0;
    VkDescriptorBufferInfo camera{};std::array<VkBuffer,2> matrices{};
    uint32_t count=0,instances=0,first=0,first_instance=0;int32_t vertex_offset=0;bool indexed=false;
};
struct XrRootAliasProbeResult {
    bool recorded=false,verified=false,indirect_guard_verified=false;uint64_t allocated_bytes=0;
    std::array<uint32_t,2> indirect_counts{};
    std::array<uint64_t,2> aliases{},uncovered{},errors{};
};
struct XrRootAliasProbe;
// Allocate only; source buffers remain owned by the caller on the recording queue.
XrRootAliasProbe* xr_root_alias_probe_create(const XrRootAliasProbeInput&);
// Outside rendering, same recording: copies source pages, overlays private camera
// bytes, patches private roots, then reads the camera only through owned pages.
bool xr_root_alias_probe_record(XrRootAliasProbe*);
struct XrRootIndirectCommand {VkBuffer buffer=VK_NULL_HANDLE;uint64_t offset=0;};
// Borrow only within the same recorded source scope. Output stays unchanged on
// rejection. Caller orders compute writes before DRAW_INDIRECT reads and keeps
// this probe alive until the recording completes. No GPU wait or host read.
bool xr_root_alias_probe_indirect_command(XrRootAliasProbe*,VkCommandBuffer,uint64_t recording,unsigned eye,XrRootIndirectCommand&);
// No wait. Expected bytes are the already-validated GPU matrix payloads.
// Null expectations retire an abandoned probe but can never verify it.
bool xr_root_alias_probe_retire(XrRootAliasProbe*&,uint64_t completed,
    const std::array<const unsigned char*,2>& expected,size_t bytes,XrRootAliasProbeResult&);
}
