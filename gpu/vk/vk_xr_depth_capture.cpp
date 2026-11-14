// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "xr_depth_capture.h"
#include "xr_gpu_scene_source.h"
#include "xr_gpu_depth_pair_probe.h"
#include "xr_gpu_eye_stream.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <chrono>
#include <charconv>
#ifdef _WIN32
#include <windows.h>
#endif
#include "vk_work.h"
namespace mkwvk {
namespace {
struct Candidate {const Texture* color=nullptr;const Texture* depth=nullptr;uint64_t generation=0;std::array<float,16> projection{};std::array<float,12> view{};uint32_t view_format=0;};
struct Capture {
    std::mutex mutex;std::vector<Candidate> candidates;uint64_t frame=UINT64_MAX,last_poll=UINT64_MAX,value=0;
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;VmaAllocation memory=nullptr;void* mapped=nullptr;
    uint32_t width=0,height=0;std::array<float,16> projection{};std::array<float,12> view{};uint32_t view_format=0;
    bool attempted=false,recorded=false,complete=false,armed=false;
    uint64_t capacity=0,next_due=0;uint32_t completed_frames=0,allocations=0;uint64_t max_publish_us=0;
};
Capture& capture(){static auto* state=new Capture;return *state;}
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_DEPTH_CAPTURE_FILE");return path&&*path?path:nullptr;}
bool streaming(){static const bool value=env_on("RX_VK_XR_DEPTH_STREAM");return value;}
uint64_t now_ns(){return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
uint32_t setting(const char* name,uint32_t fallback,uint32_t maximum){const char* value=std::getenv(name);if(!value)return fallback;uint32_t out=0;const char* end=value+std::strlen(value);const auto parsed=std::from_chars(value,end,out);return parsed.ec==std::errc{}&&parsed.ptr==end&&out&&out<=maximum?out:fallback;}
bool replace_file(const std::filesystem::path& temporary,const std::filesystem::path& target){
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING)!=0;
#else
    std::error_code error;std::filesystem::rename(temporary,target,error);return !error;
#endif
}
bool publish(const Capture& p,uint64_t bytes){
    const auto target=std::filesystem::u8path(output()),temporary=std::filesystem::u8path(std::string(output())+".tmp");
    std::ofstream file(temporary,std::ios::binary|std::ios::trunc);const char magic[8]={'H','S','V','R','D','2',0,0};
    file.write(magic,8);file.write(reinterpret_cast<const char*>(&p.width),4);file.write(reinterpret_cast<const char*>(&p.height),4);file.write(reinterpret_cast<const char*>(&p.view_format),4);
    file.write(reinterpret_cast<const char*>(p.projection.data()),64);file.write(reinterpret_cast<const char*>(p.view.data()),48);file.write(static_cast<const char*>(p.mapped),std::streamsize(bytes));file.close();if(!file)return false;
    return replace_file(temporary,target);
}
void status(const Capture& p,const char* state){
    try{const auto temporary=std::filesystem::u8path(std::string(output())+".json.tmp");std::ofstream file(temporary,std::ios::trunc);
        if(file)file<<"{\"schema\":1,\"status\":\""<<state<<"\",\"width\":"<<p.width<<",\"height\":"<<p.height<<",\"color_view_format\":"<<p.view_format<<",\"recording\":"<<p.value
          <<",\"gpu_complete\":"<<(p.complete?"true":"false")<<",\"readback_allocations\":"<<p.allocations<<",\"readback_capacity_bytes\":"<<p.capacity<<",\"max_publish_us\":"<<p.max_publish_us<<",\"completed_frames\":"<<p.completed_frames<<",\"streaming\":"<<(streaming()?"true":"false")<<",\"frozen_depth_preview\":"<<(streaming()?"false":"true")<<",\"native_stereo_render\":false}\n";
        file.close();if(file)replace_file(temporary,std::filesystem::u8path(std::string(output())+".json"));
    }catch(...){}
}
bool shape(const Texture* t,VkFormat format){return t&&t->image&&t->allocation&&!t->sparse&&t->type==VK_IMAGE_TYPE_2D&&t->format==format&&t->samples==VK_SAMPLE_COUNT_1_BIT&&t->depth==1&&t->levels==1&&t->layers==1&&t->width&&t->height&&t->width<=1920&&t->height<=1080&&(t->usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT);}
}
bool xr_depth_capture_enabled(){static const bool on=env_on("RX_VK_XR_DEPTH_CAPTURE")&&output();return on;}
void xr_depth_capture_tick(uint64_t frame){
    xr_gpu_scene_source_tick(frame);
    xr_gpu_depth_pair_probe_tick(frame);
    xr_gpu_eye_stream_tick(frame);
    if(!xr_depth_capture_enabled())return;auto& p=capture();std::lock_guard lock(p.mutex);
    if(p.last_poll==frame)return;p.last_poll=frame;
    if(!p.armed){const char* arm=std::getenv("HOOKSHOT_VR_DEPTH_CAPTURE_ARM_FILE");std::error_code error;p.armed=arm&&std::filesystem::is_regular_file(std::filesystem::u8path(arm),error);}
    if(!p.recorded||(p.complete&&!streaming())||(p.complete&&now_ns()<p.next_due))return;
    if(p.device!=dev.device){status(p,"device_changed_retained");return;}
    if(vk_timeline_completed()<p.value)return;p.complete=true;
    const uint64_t plane=uint64_t(p.width)*p.height*4;invalidate_mapped(p.memory,2*plane);
    const auto started=now_ns();bool written=false;try{written=publish(p,2*plane);}catch(...){}
    p.max_publish_us=std::max(p.max_publish_us,(now_ns()-started)/1000);
    if(!written&&streaming()){p.next_due=now_ns()+50000000;status(p,"publication_retry");return;}
    if(written)++p.completed_frames;
    if(streaming()&&written&&p.completed_frames<setting("HOOKSHOT_VR_DEPTH_MAX_FRAMES",300,300)){
        status(p,"streaming");p.attempted=false;p.recorded=false;p.complete=false;p.frame=UINT64_MAX;p.candidates.clear();
        p.next_due=now_ns()+uint64_t(std::max(100u,setting("HOOKSHOT_VR_DEPTH_INTERVAL_MS",200,1000)))*1000000;return;
    }
    destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;p.mapped=nullptr;p.capacity=0;p.recorded=false;
    status(p,written?(streaming()?"stream_complete":"completed"):"file_write_failed");
}
void xr_depth_capture_note(const Texture* color,const Texture* depth,const float* camera,size_t count){
    xr_gpu_scene_source_note(color,depth,camera,count);
    if(!xr_depth_capture_enabled())return;auto& p=capture();std::lock_guard lock(p.mutex);if(p.attempted||!camera||count<0x230/4||par_active)return;
    if(!shape(color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!shape(depth,VK_FORMAT_D32_SFLOAT)||color->width!=depth->width||color->height!=depth->height||color->image==depth->image)return;
    const auto frame=texture_frame_no();if(p.frame!=frame){p.frame=frame;p.candidates.clear();}
    Candidate next{color,depth,texture_destroy_generation()};std::copy(camera+0x70/4,camera+0xb0/4,next.projection.begin());std::copy(camera,camera+12,next.view.begin());
    for(auto& item:p.candidates)if(item.color==color){item=next;return;}
    if(p.candidates.size()<16)p.candidates.push_back(next);
}
void xr_depth_capture_view(const Texture* texture,VkFormat format){
    xr_gpu_scene_source_view(texture,format);
    if(!xr_depth_capture_enabled()||!texture)return;auto& p=capture();std::lock_guard lock(p.mutex);if(p.attempted||p.frame!=texture_frame_no())return;
    for(auto& item:p.candidates)if(item.color==texture)item.view_format=uint32_t(format);
}
void xr_depth_capture_network(VkCommandBuffer command,uint64_t recording,uint64_t program,const std::vector<const Texture*>& images){
    xr_gpu_scene_source_network(command,recording,program,images);
    xr_gpu_depth_pair_probe_network(command,recording,program);
    xr_gpu_eye_stream_network(command,recording,program);
    if(!xr_depth_capture_enabled()||(program!=0xfd029a317a9a4b94ull&&program!=0x9dc98871eb7fc101ull))return;
    auto& p=capture();std::lock_guard lock(p.mutex);
    if(!p.armed||p.attempted||(streaming()&&now_ns()<p.next_due)||par_active||!command||!recording||recording<=vk_timeline_completed()||p.frame!=texture_frame_no())return;
    for(const auto& item:p.candidates){
        if(item.generation!=texture_destroy_generation()||std::find(images.begin(),images.end(),item.color)==images.end())continue;
        if(!shape(item.color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!shape(item.depth,VK_FORMAT_D32_SFLOAT))continue;
        if(item.view_format!=VK_FORMAT_E5B9G9R9_UFLOAT_PACK32&&item.view_format!=VK_FORMAT_B10G11R11_UFLOAT_PACK32)continue;
        p.view_format=item.view_format;
        p.attempted=true;p.device=dev.device;p.value=recording;p.width=item.color->width;p.height=item.color->height;p.projection=item.projection;p.view=item.view;
        const uint64_t plane=uint64_t(p.width)*p.height*4;
        if(p.buffer&&p.capacity<2*plane){destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;p.mapped=nullptr;p.capacity=0;}
        const bool allocate=!p.buffer;
        if(!p.buffer&&(!create_buffer_cached(2*plane,VK_BUFFER_USAGE_TRANSFER_DST_BIT,p.buffer,p.memory,&p.mapped,true)||!p.mapped)){if(p.buffer)destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;status(p,"allocation_failed");return;}
        if(!allocation_size(p.memory)||allocation_size(p.memory)>16ull*1024*1024){destroy_buffer(p.buffer,p.memory);p.buffer=VK_NULL_HANDLE;p.memory=nullptr;status(p,"allocation_budget");return;}
        if(allocate)++p.allocations;
        p.capacity=std::max(p.capacity,2*plane);p.next_due=0;
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
