// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "xr_eye_image_factory.h"
#include "xr_eye_image_probe.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
namespace mkwvk {
namespace {
struct Probe {
    std::mutex mutex;EyeImageClonePair color,depth;
    VkDevice device=VK_NULL_HANDLE;uint64_t value=0,bytes=0,last_poll=UINT64_MAX;
    bool attempted=false,recorded=false,complete=false,released=false;
    uint32_t width=0,height=0;int color_format=0,depth_format=0;
};
Probe& probe(){static auto* state=new Probe;return *state;}
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_EYE_IMAGE_PROBE_FILE");return path&&*path?path:nullptr;}
void report(const Probe& p,const char* status){
    std::ofstream file(std::filesystem::u8path(output()),std::ios::trunc);
    if(file)file<<"{\"schema\":1,\"status\":\""<<status<<"\",\"width\":"<<p.width<<",\"height\":"<<p.height
        <<",\"color_format\":"<<p.color_format<<",\"depth_format\":"<<p.depth_format<<",\"allocated_bytes\":"<<p.bytes
        <<",\"copy_recorded\":"<<(p.recorded?"true":"false")<<",\"gpu_completion_verified\":"<<(p.complete?"true":"false")
        <<",\"resources_released\":"<<(p.released?"true":"false")<<",\"source_objects_replaced\":false,\"stereo_replay_enabled\":false}\n";
}
bool allocate(const VkImageCreateInfo& ci,VkImage& image,uintptr_t& allocation){
    VkImage candidate=VK_NULL_HANDLE;VmaAllocation memory=nullptr;
    if(!create_xr_private_image(ci,candidate,memory))return false;
    image=candidate;allocation=reinterpret_cast<uintptr_t>(memory);return true;
}
void release(VkImage image,uintptr_t allocation){destroy_image(image,reinterpret_cast<VmaAllocation>(allocation));}
uint64_t bytes(uintptr_t allocation){return allocation_size(reinterpret_cast<VmaAllocation>(allocation));}
EyeImageSource describe(const Texture& t){EyeImageSource s;s.image=t.image;s.identity=t.id;s.format=t.format;s.aspect=t.aspect;
    s.flags=t.create_flags;s.usage=t.usage;s.width=t.width;s.height=t.height;s.layers=t.layers;s.levels=t.levels;
    s.sparse=t.sparse;s.samples=t.samples;s.type=t.type;return s;}
}
bool xr_eye_image_probe_wants(uint64_t frame){
    if(!output())return false;
    auto& p=probe();std::lock_guard lock(p.mutex);
    if(!p.attempted)return true;
    if(p.recorded&&!p.complete&&p.last_poll!=frame){p.last_poll=frame;
        if(p.device!=dev.device){report(p,"device_changed_resources_retained");return false;}
        const auto completed=vk_timeline_completed();
        if(completed>=p.value){
            p.complete=true;p.released=p.color.retire(completed)&&p.depth.retire(completed);
            report(p,"completed");logf("vr-eye-images: native color/depth copies completed and released (%llu bytes)",(unsigned long long)p.bytes);
        }
    }
    return false;
}
void xr_eye_image_probe_record(VkCommandBuffer command,const Texture* color,const Texture* depth,uint64_t recording_value){
    if(!output())return;
    auto& p=probe();std::lock_guard lock(p.mutex);if(p.attempted)return;p.attempted=true;p.device=dev.device;
    if(!command||!color||!depth||!color->image||!depth->image||color->depth!=1||depth->depth!=1){report(p,"invalid_source");return;}
    p.value=recording_value;p.width=color->width;p.height=color->height;
    p.color_format=int(color->format);p.depth_format=int(depth->format);
    if(!p.value||vk_timeline_completed()>=p.value){report(p,"invalid_recording_value");return;}
    EyeImageFactoryApi api{dev.device,vkCreateImage,vkDestroyImage,vkGetImageMemoryRequirements,allocate,release,bytes,vkCmdPipelineBarrier2,vkCmdCopyImage};
    constexpr uint64_t budget=32ull*1024*1024;
    if(!p.color.create(describe(*color),api,budget)||!p.depth.create(describe(*depth),api,budget-p.color.allocated_bytes())){
        p.color.retire(0);p.depth.retire(0);report(p,"allocation_or_shape_rejected");return;
    }
    p.bytes=p.color.allocated_bytes()+p.depth.allocated_bytes();
    const bool c=p.color.record_initial_copy(command,p.value),d=p.depth.record_initial_copy(command,p.value);
    p.recorded=c&&d;report(p,p.recorded?"recorded":"copy_recording_failed");
    logf("vr-eye-images: native color/depth pair copies recorded (%llu bytes, original images retained)",(unsigned long long)p.bytes);
}
}
