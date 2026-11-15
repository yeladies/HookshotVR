// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdlib>
#include <cstring>
#include <initializer_list>
namespace mkwvk {
inline bool xr_retired_camera_requested(){
#ifndef HOOKSHOT_VR_LEGACY_FIXTURES
    for(const char* key:{"RX_VK_XR_IMMERSIVE","RX_VK_XR_GPU_WORLD_PREVIEW","RX_VK_XR_GPU_LATE_REPROJECT",
                         "HOOKSHOT_VR_IMMERSIVE_TEST","RX_VK_XR_TEMPORAL_STEREO","RX_VK_XR_WIDE_FOV",
                         "HOOKSHOT_VR_COHERENT_VIEW"}){
        const char* value=std::getenv(key);if(value&&!std::strcmp(value,"1"))return true;
    }
#endif
    return false;
}
}
