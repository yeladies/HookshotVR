// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
namespace mkwvk { void logf(const char*, ...) {} }
int main() {
    using namespace mkwvk;
    if (!xr_vulkan_prepare(false) || xr_vulkan_active()) return 1;
    if (xr_vulkan_prepare(true) || xr_vulkan_active()) return 2;
    xr_vulkan_reset();
    return xr_vulkan_prepare(false) ? 0 : 3;
}