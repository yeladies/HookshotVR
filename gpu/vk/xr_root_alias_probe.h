// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_uniform_copy_source.h"
#include <array>
namespace mkwvk {
struct XrRootAliasProbeInput {
    VkCommandBuffer command=VK_NULL_HANDLE;uint64_t recording=0,guest_camera=0;
    VkDescriptorBufferInfo camera{};std::array<VkBuffer,2> matrices{};
};
struct XrRootAliasProbeResult {
    bool recorded=false,verified=false;uint64_t allocated_bytes=0;
    std::array<uint64_t,2> aliases{},uncovered{},errors{};
};
struct XrRootAliasProbe;
// Allocate only; source buffers remain owned by the caller on the recording queue.
XrRootAliasProbe* xr_root_alias_probe_create(const XrRootAliasProbeInput&);
// Outside rendering, same recording: copies source pages, overlays private camera
// bytes, patches private roots, then reads the camera only through owned pages.
bool xr_root_alias_probe_record(XrRootAliasProbe*);
// No wait. Expected bytes are the already-validated GPU matrix payloads.
// Null expectations retire an abandoned probe but can never verify it.
bool xr_root_alias_probe_retire(XrRootAliasProbe*&,uint64_t completed,
    const std::array<const unsigned char*,2>& expected,size_t bytes,XrRootAliasProbeResult&);
}
