// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "xr_depth_capture.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "vk_work.h"
namespace mkwvk {
namespace {
struct Candidate {const Texture* color=nullptr;const Texture* depth=nullptr;uint64_t generation=0;std::array<float,16> projection{};std::array<float,12> view{};uint32_t view_format=0;};
struct Capture {
    std::mutex mutex;std::vector<Candidate> candidates;uint64_t frame=UINT64_MAX,last_poll=UINT64_MAX,value=0;
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;VmaAllocation memory=nullptr;void* mapped=nullptr;
    uint32_t width=0,height=0;std::array<float,16> projection{};std::array<float,12> view{};uint32_t view_format=0;
    bool attempted=false,recorded=false,complete=false,armed=false;
};
Capture& capture(){static auto* state=new Capture;return *state;}
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_DEPTH_CAPTURE_FILE");return path&&*path?path:nullptr;}
void status(const Capture& p,const char* state){
    try{std::ofstream file(std::filesystem::u8path(std::string(output())+".json"),std::ios::trunc);
        if(file)file<<"{\"schema\":1,\"status\":\""<<state<<"\",\"width\":"<<p.width<<",\"height\":"<<p.height<<",\"color_view_format\":"<<p.view_format<<",\"recording\":"<<p.value
          <<",\"gpu_complete\":"<<(p.complete?"true":"false")<<",\"frozen_depth_preview\":true,\"native_stereo_render\":false}\n";
    }catch(...){}
}
bool shape(const Texture* t,VkFormat format){return t&&t->image&&t->allocation&&!t->sparse&&t->type==VK_IMAGE_TYPE_2D&&t->format==format&&t->samples==VK_SAMPLE_COUNT_1_BIT&&t->depth==1&&t->levels==1&&t->layers==1&&t->width&&t->height&&t->width<=1920&&t->height<=1080&&(t->usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT);}
}
bool xr_depth_capture_enabled(){static const bool on=env_on("RX_VK_XR_DEPTH_CAPTURE")&&output();return on;}
void xr_depth_capture_tick(uint64_t frame){
    if(!xr_depth_capture_enabled())return;auto& p=capture();std::lock_guard lock(p.mutex);
    if(p.last_poll==frame)return;p.last_poll=frame;
    if(!p.armed){const char* arm=std::getenv("HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE");std::error_code error;p.armed=arm&&std::filesystem::is_regular_file(std::filesystem::u8path(arm),error);}
    if(!p.recorded||p.complete)return;
    if(p.device!=dev.device){status(p,"device_changed_retained");return;}
    if(vk_timeline_completed()<p.value)return;p.complete=true;
    const uint64_t plane=uint64_t(p.width)*p.height*4;invalidate_mapped(p.memory,2*plane);
    bool written=false;
    try{std::ofstream file(std::filesystem::u8path(output()),std::ios::binary|std::ios::trunc);
        const char magic[8]={'H','S','V','R','D','2',0,0};file.write(magic,8);file.write(reinterpret_cast<const char*>(&p.width),4);file.write(reinterpret_cast<const char*>(&p.height),4);file.write(reinterpret_cast<const char*>(&p.view_format),4);
        file.write(reinterpret_cast<const char*>(p.projection.data()),64);file.write(reinterpret_cast<const char*>(p.view.data()),48);file.write(static_cast<const char*>(p.mapped),std::streamsize(2*plane));file.close();written=bool(file);
    }catch(...){}
    destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;p.mapped=nullptr;
    status(p,written?"completed":"file_write_failed");
}
void xr_depth_capture_note(const Texture* color,const Texture* depth,const float* camera,size_t count){
    if(!xr_depth_capture_enabled())return;auto& p=capture();std::lock_guard lock(p.mutex);if(p.attempted||!camera||count<0x230/4||par_active)return;
    if(!shape(color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!shape(depth,VK_FORMAT_D32_SFLOAT)||color->width!=depth->width||color->height!=depth->height||color->image==depth->image)return;
    const auto frame=texture_frame_no();if(p.frame!=frame){p.frame=frame;p.candidates.clear();}
    Candidate next{color,depth,texture_destroy_generation()};std::copy(camera+0x70/4,camera+0xb0/4,next.projection.begin());std::copy(camera,camera+12,next.view.begin());
    for(auto& item:p.candidates)if(item.color==color){item=next;return;}
    if(p.candidates.size()<16)p.candidates.push_back(next);
}
void xr_depth_capture_view(const Texture* texture,VkFormat format){
    if(!xr_depth_capture_enabled()||!texture)return;auto& p=capture();std::lock_guard lock(p.mutex);if(p.attempted||p.frame!=texture_frame_no())return;
    for(auto& item:p.candidates)if(item.color==texture)item.view_format=uint32_t(format);
}
void xr_depth_capture_network(VkCommandBuffer command,uint64_t recording,uint64_t program,const std::vector<const Texture*>& images){
    if(!xr_depth_capture_enabled()||(program!=0xfd029a317a9a4b94ull&&program!=0x9dc98871eb7fc101ull))return;
    auto& p=capture();std::lock_guard lock(p.mutex);
    if(!p.armed||p.attempted||par_active||!command||!recording||recording<=vk_timeline_completed()||p.frame!=texture_frame_no())return;
    for(const auto& item:p.candidates){
        if(item.generation!=texture_destroy_generation()||std::find(images.begin(),images.end(),item.color)==images.end())continue;
        if(!shape(item.color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!shape(item.depth,VK_FORMAT_D32_SFLOAT))continue;
        if(item.view_format!=VK_FORMAT_E5B9G9R9_UFLOAT_PACK32&&item.view_format!=VK_FORMAT_B10G11R11_UFLOAT_PACK32)continue;
        p.view_format=item.view_format;
        p.attempted=true;p.device=dev.device;p.value=recording;p.width=item.color->width;p.height=item.color->height;p.projection=item.projection;p.view=item.view;
        const uint64_t plane=uint64_t(p.width)*p.height*4;
        if(!create_buffer(2*plane,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true,p.buffer,p.memory,&p.mapped,true)||!p.mapped){if(p.buffer)destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;status(p,"allocation_failed");return;}
        if(allocation_size(p.memory)>16ull*1024*1024){destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;status(p,"allocation_budget");return;}
        VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};barrier.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;barrier.srcAccessMask=VK_ACCESS_2_MEMORY_WRITE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;barrier.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;
        VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dep.memoryBarrierCount=1;dep.pMemoryBarriers=&barrier;vkCmdPipelineBarrier2(command,&dep);
        unsigned index=0;for(const auto* texture:{item.color,item.depth}){VkBufferImageCopy copy{};copy.bufferOffset=index++*plane;copy.imageSubresource={texture->aspect,0,0,1};copy.imageExtent={p.width,p.height,1};
            vkCmdCopyImageToBuffer(command,texture->image,VK_IMAGE_LAYOUT_GENERAL,p.buffer,1,&copy);}
        barrier.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;barrier.srcAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.dstStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT;barrier.dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT;vkCmdPipelineBarrier2(command,&dep);
        p.recorded=true;status(p,"recorded");return;
    }
}
}
