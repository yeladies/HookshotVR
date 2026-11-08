// CPU-only regression for an observed washed-out headset image. No XR calls.
#include "panel_color.h"
#include <cstdio>
#include <cstring>

int main() {
    // Runtime preference order must not cause encoded desktop bytes to be
    // submitted in a linear UNORM image (the original excessive-brightness bug).
    const std::vector<int64_t> mixed{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB};
    if (choose_panel_format(mixed, true) != VK_FORMAT_B8G8R8A8_SRGB) return 1;
    if (choose_panel_format({VK_FORMAT_R8G8B8A8_SRGB}, true) != VK_FORMAT_R8G8B8A8_SRGB) return 2;
    if (choose_panel_format({VK_FORMAT_B8G8R8A8_UNORM}, true) != VK_FORMAT_UNDEFINED) return 3;
    if (choose_panel_format(mixed, false) != VK_FORMAT_R8G8B8A8_UNORM) return 4;
    const uint8_t bgra[4] = {15, 128, 240, 0};
    uint8_t result[4]{};
    copy_capture_pixel(result, bgra, panel_is_bgra(VK_FORMAT_B8G8R8A8_SRGB));
    const uint8_t expected_bgra[4] = {15, 128, 240, 255};
    if (std::memcmp(result, expected_bgra, 4)) return 5;
    copy_capture_pixel(result, bgra, panel_is_bgra(VK_FORMAT_R8G8B8A8_SRGB));
    const uint8_t expected_rgba[4] = {240, 128, 15, 255};
    if (std::memcmp(result, expected_rgba, 4)) return 6;
    std::puts("Encoded SDR bytes, sRGB selection and channel order passed");
    return 0;
}
