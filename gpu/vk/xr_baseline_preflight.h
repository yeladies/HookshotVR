// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace mkwvk {
struct XrBaselinePreflightNote {
    uint64_t program=0,recording=0,frame=0;
    uint32_t width=0,height=0,uniform_bytes=0,camera_bindings=0,read_images=0;
    const char* reason="unexamined";
};
bool xr_baseline_preflight_enabled();
void xr_baseline_preflight_note(const XrBaselinePreflightNote&);
}
