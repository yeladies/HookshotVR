// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_internal.h"
#include "xr_baseline_preflight.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <string>
namespace mkwvk {
namespace {
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_BASELINE_PREFLIGHT_FILE");return path&&*path?path:nullptr;}
}
bool xr_baseline_preflight_enabled(){static const bool enabled=env_on("RX_VK_XR_BASELINE_PREFLIGHT")&&output();return enabled;}
void xr_baseline_preflight_note(const XrBaselinePreflightNote& n){
    if(!xr_baseline_preflight_enabled()||!n.program||!n.reason)return;
    static std::mutex mutex;static std::set<std::pair<uint64_t,std::string>> emitted;
    std::lock_guard lock(mutex);
    if(emitted.size()>=256||!emitted.emplace(n.program,n.reason).second)return;
    std::ofstream file(std::filesystem::u8path(output()),std::ios::app);
    if(file)file<<"{\"schema\":1,\"program\":\""<<std::hex<<n.program<<std::dec<<"\",\"recording\":"<<n.recording
        <<",\"frame\":"<<n.frame<<",\"width\":"<<n.width<<",\"height\":"<<n.height<<",\"uniform_bytes\":"<<n.uniform_bytes
        <<",\"camera_bindings\":"<<n.camera_bindings<<",\"read_images\":"<<n.read_images<<",\"reason\":\""<<n.reason
        <<"\",\"extra_draw_recorded\":false,\"shader_effects_reviewed\":false,\"safe_to_replay\":false}\n";
}
}
