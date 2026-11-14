// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include "xr_immersive_camera.h"
#include <array>
#include <memory>
#include <vector>
namespace mkwvk {
struct Texture;
// A const lease pins a completed immutable GPU packet. Consumers must keep it
// until their own GPU use completes, including on timeout/device-loss retention.
struct XrGpuSceneSource {
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;
    uint64_t address=0,bytes=0,allocated_bytes=0,state=0,recording=0,generation=0,replay_frame=0;
    uint32_t width=0,height=0,format=0,queue_family=0;
    int64_t captured_ns=0,ready_ns=0;
    std::array<float,16> projection{};std::array<float,12> view{};
    ImmersiveTag pose;bool synthetic_pose=false;
private:
    std::shared_ptr<const void> lifetime_;
    friend struct XrGpuSceneSourceAccess;

};
bool xr_gpu_scene_source_enabled();
void xr_gpu_scene_source_tick(uint64_t frame);
void xr_gpu_scene_source_note(const Texture*,const Texture*,const float* camera,size_t count);
void xr_gpu_scene_source_view(const Texture*,VkFormat);
void xr_gpu_scene_source_network(VkCommandBuffer,uint64_t recording,uint64_t program,const std::vector<const Texture*>&);
std::shared_ptr<const XrGpuSceneSource> xr_gpu_scene_source_acquire(VkDevice,uint64_t origin,int64_t now,bool require_pose=true,bool allow_synthetic=false);
// Stops publication. In-flight, borrowed or uncertain-device buffers are retained.
bool xr_gpu_scene_source_stop();
}
