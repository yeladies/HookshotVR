#pragma once
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

// SDR desktop captures already contain sRGB-encoded bytes. Keep them encoded
// and require an sRGB XR image so the compositor performs the correct decode.
inline VkFormat choose_panel_format(const std::vector<int64_t>& formats, bool capture) {
    const VkFormat first = capture ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM;
    const VkFormat second = capture ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    for (auto want : {first, second})
        for (auto supported : formats)
            if (supported == want) return want;
    return VK_FORMAT_UNDEFINED;
}
inline bool panel_is_bgra(VkFormat format) {
    return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB;
}
inline void copy_capture_pixel(uint8_t* dst, const uint8_t* src, bool bgra) {
    dst[0] = src[bgra ? 0 : 2];
    dst[1] = src[1];
    dst[2] = src[bgra ? 2 : 0];
    dst[3] = 255;
}
