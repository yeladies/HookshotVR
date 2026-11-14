// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "vk_xr.h"
#include "xr_gpu_scene_source.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "vk_work.h"
namespace mkwvk {
struct XrGpuSceneSourceAccess {static void pin(XrGpuSceneSource& frame,const std::shared_ptr<const void>& token){frame.lifetime_=token;}};
namespace {
constexpr uint64_t gpu_scene_packet_cap=16ull*1024*1024,gpu_scene_pool_cap=64ull*1024*1024;
int64_t gpu_scene_now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct GpuSceneBuffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;uint64_t capacity=0,allocated=0,address=0;void* mapped=nullptr;bool coherent=false;};
struct GpuSceneCandidate {const Texture* color=nullptr;const Texture* depth=nullptr;uint64_t texture_generation=0;uint32_t format=0;std::array<float,16> projection{};std::array<float,12> view{};ImmersiveTag pose;bool synthetic=false;};
struct GpuScenePin {};
struct GpuSceneSlot {GpuSceneBuffer storage;std::shared_ptr<const void> pin=std::make_shared<GpuScenePin>();std::shared_ptr<XrGpuSceneSource> frame;bool pending=false;};
struct GpuScenePool {
    PFN_vkUnmapMemory unmap=nullptr;PFN_vkInvalidateMappedMemoryRanges invalidate=nullptr;
    std::mutex mutex;VkDevice device=VK_NULL_HANDLE;uint64_t generation=0,frame=UINT64_MAX,last_tick=UINT64_MAX,last_record=UINT64_MAX,sequence=0,allocated=0;
    std::vector<GpuSceneCandidate> candidates;std::array<GpuSceneSlot,3> slots;std::shared_ptr<const XrGpuSceneSource> latest;
    uint64_t captures=0,completed=0,published=0,leased_skips=0,allocations=0,failures=0;bool armed=false,stopped=false,retained=false;int64_t arm_poll=0;
    GpuSceneBuffer probe;std::shared_ptr<const XrGpuSceneSource> probe_source;uint64_t probe_recording=0;bool probe_pending=false,probe_complete=false,probe_attempted=false;
};
GpuScenePool& gpu_scene_pool(){static auto* p=new GpuScenePool;return *p;}
void gpu_scene_report(const GpuScenePool& p,const char* status){
    const char* path=std::getenv("HOOKSHOT_VR_GPU_SOURCE_REPORT_FILE");if(!path||!*path)return;
    try{std::ofstream file(std::filesystem::u8path(path),std::ios::trunc);if(file)file<<"{\"schema\":1,\"status\":\""<<status<<"\",\"captures\":"<<p.captures<<",\"completed\":"<<p.completed<<",\"published\":"<<p.published<<",\"leased_skips\":"<<p.leased_skips<<",\"allocations\":"<<p.allocations<<",\"allocated_bytes\":"<<p.allocated<<",\"failures\":"<<p.failures<<",\"device_generation\":"<<p.generation<<",\"latest_state\":"<<(p.latest?p.latest->state:0)<<",\"probe_complete\":"<<(p.probe_complete?"true":"false")<<",\"source_publication_CPU_pixels\":false,\"validation_packet_readback\":"<<(p.probe_complete?"true":"false")<<",\"XR_consumer_connected\":false,\"native_stereo\":false}\n";}catch(...){}
}
void gpu_scene_destroy(GpuScenePool& p,GpuSceneBuffer& b){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);p.allocated-=b.allocated;b={};}
bool gpu_scene_allocate(GpuScenePool& p,uint64_t bytes,bool host,GpuSceneBuffer& out){
    if(!bytes||bytes>gpu_scene_packet_cap)return false;GpuSceneBuffer b;
    VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};create.size=bytes;create.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(!host)create.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if(vkCreateBuffer(p.device,&create,nullptr,&b.buffer)!=VK_SUCCESS)return false;
    VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(p.device,b.buffer,&requirements);
    auto fail=[&](){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);return false;};
    if(!requirements.size||requirements.size>gpu_scene_packet_cap||requirements.size>gpu_scene_pool_cap-p.allocated)return fail();
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(dev.physical,&memory);uint32_t type=UINT32_MAX;
    const auto wanted=host?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&wanted)==wanted){type=i;break;}
    if(type==UINT32_MAX)return fail();b.coherent=(memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=host?nullptr:&flags;allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
    if(vkAllocateMemory(p.device,&allocation,nullptr,&b.memory)!=VK_SUCCESS||vkBindBufferMemory(p.device,b.buffer,b.memory,0)!=VK_SUCCESS)return fail();
    if(host){if(vkMapMemory(p.device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped)!=VK_SUCCESS||!b.mapped)return fail();}
    else{VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.buffer;b.address=vkGetBufferDeviceAddress(p.device,&info);if(!b.address||b.address%16||b.address>UINT64_MAX-bytes)return fail();}
    b.capacity=bytes;b.allocated=requirements.size;p.allocated+=b.allocated;out=b;++p.allocations;return true;
}
bool gpu_scene_shape(const Texture* t,VkFormat format){return t&&t->image&&t->allocation&&!t->sparse&&t->type==VK_IMAGE_TYPE_2D&&t->format==format&&t->samples==VK_SAMPLE_COUNT_1_BIT&&t->depth==1&&t->levels==1&&t->layers==1&&t->width>=2&&t->height>=2&&t->width<=1920&&t->height<=1080&&(t->usage&VK_IMAGE_USAGE_TRANSFER_SRC_BIT);}
bool gpu_scene_camera(const float* c,size_t count){
    if(!c||count<0x230/4)return false;for(unsigned i=0;i<44;++i)if(!std::isfinite(c[i]))return false;const auto* p=c+28;
    if(p[0]<.0001f||p[5]<.0001f||p[0]>10000||p[5]>10000||std::abs(p[2])>4||std::abs(p[6])>4||p[10]>-1||p[11]>=0||std::abs(p[14]+1)>1e-5)return false;
    for(unsigned i:{1,3,4,7,8,9,12,13,15})if(std::abs(p[i])>1e-5)return false;
    for(unsigned a=0;a<3;++a)for(unsigned b=0;b<3;++b){double dot=0;for(unsigned k=0;k<3;++k)dot+=double(c[a*4+k])*c[b*4+k];if(std::abs(dot-(a==b))>.003)return false;}
    const double det=c[0]*(double(c[5])*c[10]-double(c[6])*c[9])-c[1]*(double(c[4])*c[10]-double(c[6])*c[8])+c[2]*(double(c[4])*c[9]-double(c[5])*c[8]);if(std::abs(det-1)>.003)return false;
    for(unsigned r=0;r<4;++r)for(unsigned col=0;col<4;++col){double value=0;for(unsigned k=0;k<4;++k)value+=double(p[r*4+k])*(k==3?(col==3?1:0):c[k*4+col]);if(std::abs(value-c[12+r*4+col])>.02+std::abs(value)*.0001)return false;}return true;
}
void gpu_scene_order(VkCommandBuffer command,VkPipelineStageFlags2 stage,VkAccessFlags2 access){VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstStageMask=stage;b.dstAccessMask=access;VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.memoryBarrierCount=1;d.pMemoryBarriers=&b;vkCmdPipelineBarrier2(command,&d);}
bool gpu_scene_probe_write(const GpuScenePool& p){
    const char* path=std::getenv("HOOKSHOT_VR_GPU_SOURCE_PROBE_FILE");if(!path||!*path||!p.probe_source)return false;
    try{std::ofstream file(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);const char magic[8]={'H','S','V','R','D','2',0,0};const auto& f=*p.probe_source;
        file.write(magic,8);file.write(reinterpret_cast<const char*>(&f.width),4);file.write(reinterpret_cast<const char*>(&f.height),4);file.write(reinterpret_cast<const char*>(&f.format),4);file.write(reinterpret_cast<const char*>(f.projection.data()),64);file.write(reinterpret_cast<const char*>(f.view.data()),48);file.write(static_cast<const char*>(p.probe.mapped),std::streamsize(f.bytes));file.close();return bool(file);
    }catch(...){return false;}
}
}
bool xr_gpu_scene_source_enabled(){static const bool on=env_on("RX_VK_XR_GPU_SCENE_SOURCE");return on;}
void xr_gpu_scene_source_tick(uint64_t frame){
    if(!xr_gpu_scene_source_enabled())return;auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);if(p.stopped||p.retained||p.last_tick==frame)return;p.last_tick=frame;
    if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();gpu_scene_report(p,"device_changed_retained");return;}
    if(!p.armed){const char* arm=std::getenv("HOOKSHOT_VR_GPU_SOURCE_ARM_FILE");const auto now=gpu_scene_now();if(!arm||!*arm)p.armed=true;else if(now-p.arm_poll>=100000000){p.arm_poll=now;std::error_code error;p.armed=std::filesystem::is_regular_file(std::filesystem::u8path(arm),error);}}
    const auto completed=vk_timeline_completed();
    const auto prior_completed=p.completed;
    for(auto& slot:p.slots)if(slot.pending&&completed>=slot.frame->recording){slot.pending=false;slot.frame->ready_ns=gpu_scene_now();++p.completed;if(!p.latest||slot.frame->state>p.latest->state){p.latest=slot.frame;++p.published;}}
    if(p.probe_pending&&completed>=p.probe_recording){if(!p.probe.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=p.probe.memory;range.size=VK_WHOLE_SIZE;if(p.invalidate(p.device,1,&range)!=VK_SUCCESS){p.retained=true;gpu_scene_report(p,"probe_invalidate_failed_retained");return;}}
        p.probe_pending=false;p.probe_complete=gpu_scene_probe_write(p);p.probe_source.reset();gpu_scene_report(p,p.probe_complete?"probe_completed":"probe_file_failed");}
    else if(p.completed!=prior_completed&&p.completed%30==0)gpu_scene_report(p,"streaming");
}
void xr_gpu_scene_source_note(const Texture* color,const Texture* depth,const float* camera,size_t count){
    if(!xr_gpu_scene_source_enabled()||par_active||!gpu_scene_shape(color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!gpu_scene_shape(depth,VK_FORMAT_D32_SFLOAT)||
       color->width!=depth->width||color->height!=depth->height||color->image==depth->image||color->allocation==depth->allocation||!camera||count<0x230/4)return;
    auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);if(p.stopped||p.retained)return;
    const auto frame=texture_frame_no();if(p.frame!=frame){p.frame=frame;p.candidates.clear();}
    for(const auto& c:p.candidates)if(c.color==color&&c.depth==depth&&!std::memcmp(c.view.data(),camera,48)&&!std::memcmp(c.projection.data(),camera+28,64))return;
    if(!gpu_scene_camera(camera,count))return;
    GpuSceneCandidate next;next.color=color;next.depth=depth;next.texture_generation=texture_destroy_generation();std::copy(camera,camera+12,next.view.begin());std::copy(camera+28,camera+44,next.projection.begin());next.pose=xr_immersive_camera_tag(camera,count,&next.synthetic);
    for(auto& c:p.candidates)if(c.color==color){c=next;return;}if(p.candidates.size()<16)p.candidates.push_back(next);
}
void xr_gpu_scene_source_view(const Texture* texture,VkFormat format){
    if(!xr_gpu_scene_source_enabled()||!texture)return;auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);if(p.stopped||p.retained||p.frame!=texture_frame_no())return;
    for(auto& candidate:p.candidates)if(candidate.color==texture)candidate.format=uint32_t(format);
}
void xr_gpu_scene_source_network(VkCommandBuffer command,uint64_t recording,uint64_t program,const std::vector<const Texture*>& images){
    if(!xr_gpu_scene_source_enabled()||(program!=0xfd029a317a9a4b94ull&&program!=0x9dc98871eb7fc101ull))return;
    auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);
    if(p.stopped||p.retained||!p.armed||par_active||!command||!recording||recording<=vk_timeline_completed()||p.frame!=texture_frame_no())return;
    if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();gpu_scene_report(p,"device_changed_retained");return;}
    if(!p.device){static std::atomic<uint64_t> epochs{0};p.device=dev.device;p.generation=++epochs;
        p.unmap=reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddr(p.device,"vkUnmapMemory"));
        p.invalidate=reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(vkGetDeviceProcAddr(p.device,"vkInvalidateMappedMemoryRanges"));
        if(!p.device||!p.unmap||!p.invalidate){p.stopped=true;++p.failures;gpu_scene_report(p,"missing_device_functions");return;}}
    const bool probe=env_on("RX_VK_XR_GPU_SCENE_SOURCE_PROBE");
    if(p.last_record!=p.frame&&(!probe||p.captures<300))for(const auto& candidate:p.candidates){
        if(candidate.texture_generation!=texture_destroy_generation()||std::find(images.begin(),images.end(),candidate.color)==images.end()||
           (candidate.format!=122&&candidate.format!=123)||!gpu_scene_shape(candidate.color,VK_FORMAT_B10G11R11_UFLOAT_PACK32)||!gpu_scene_shape(candidate.depth,VK_FORMAT_D32_SFLOAT))continue;
        GpuSceneSlot* slot=nullptr;for(auto& item:p.slots){
            if(item.pending)continue;
            // Detach the published metadata before testing the private pin. A
            // concurrent weak_ptr::lock can then either retain its old pin or
            // fail; it can never observe metadata rewritten for a newer frame.
            item.frame.reset();if(item.pin.use_count()==1){slot=&item;break;}
        }
        if(!slot){++p.leased_skips;break;}const uint64_t bytes=uint64_t(candidate.color->width)*candidate.color->height*8;
        if(slot->storage.buffer&&slot->storage.capacity<bytes)gpu_scene_destroy(p,slot->storage);
        if(!slot->storage.buffer&&!gpu_scene_allocate(p,bytes,false,slot->storage)){++p.failures;if(p.failures>=3)p.stopped=true;gpu_scene_report(p,"source_allocation_failed");break;}
        try{slot->frame=std::make_shared<XrGpuSceneSource>();}catch(...){++p.failures;gpu_scene_report(p,"metadata_allocation_failed");return;}
        auto& f=*slot->frame;XrGpuSceneSourceAccess::pin(f,slot->pin);f.device=p.device;f.buffer=slot->storage.buffer;f.address=slot->storage.address;f.bytes=bytes;f.allocated_bytes=slot->storage.allocated;
        f.state=++p.sequence;f.recording=recording;f.generation=p.generation;f.replay_frame=p.frame;f.width=candidate.color->width;f.height=candidate.color->height;f.format=candidate.format;f.queue_family=dev.queue_family;
        f.captured_ns=gpu_scene_now();f.projection=candidate.projection;f.view=candidate.view;f.pose=candidate.pose;f.synthetic_pose=candidate.synthetic;
        gpu_scene_order(command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
        unsigned plane=0;for(const auto* texture:{candidate.color,candidate.depth}){VkBufferImageCopy copy{};copy.bufferOffset=plane++*(bytes/2);copy.imageSubresource={texture->aspect,0,0,1};copy.imageExtent={f.width,f.height,1};vkCmdCopyImageToBuffer(command,texture->image,VK_IMAGE_LAYOUT_GENERAL,f.buffer,1,&copy);}
        gpu_scene_order(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT);
        slot->pending=true;p.last_record=p.frame;++p.captures;if(p.captures==1)gpu_scene_report(p,"source_recorded");break;
    }
    const char* output=std::getenv("HOOKSHOT_VR_GPU_SOURCE_PROBE_FILE");
    if(probe&&output&&*output&&p.latest&&!p.probe_attempted){
        p.probe_attempted=true;if(!gpu_scene_allocate(p,p.latest->bytes,true,p.probe)){++p.failures;gpu_scene_report(p,"probe_allocation_failed");return;}
        p.probe_source=p.latest;p.probe_recording=recording;
        gpu_scene_order(command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferCopy copy{0,0,p.probe_source->bytes};vkCmdCopyBuffer(command,p.probe_source->buffer,p.probe.buffer,1,&copy);
        gpu_scene_order(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);p.probe_pending=true;
    }
}
std::shared_ptr<const XrGpuSceneSource> xr_gpu_scene_source_acquire(VkDevice device,uint64_t origin,int64_t now,bool require_pose,bool allow_synthetic){
    if(!xr_gpu_scene_source_enabled()||!device||now<=0)return {};auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);
    if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();return {};}const auto f=p.latest;
    if(p.stopped||p.retained||!f||f->device!=device||f->generation!=p.generation||f->ready_ns<=0||now<f->captured_ns||now-f->captured_ns>1000000000)return {};
    if(require_pose&&(!origin||!f->pose.valid||f->pose.menu||f->pose.temporal_stereo||f->pose.generation!=origin||(!allow_synthetic&&f->synthetic_pose)||
       f->pose.observed_ns<=0||now<f->pose.observed_ns||now-f->pose.observed_ns>1000000000))return {};
    return f;
}
bool xr_gpu_scene_source_stop(){
    if(!xr_gpu_scene_source_enabled())return true;auto& p=gpu_scene_pool();std::lock_guard lock(p.mutex);p.stopped=true;p.latest.reset();
    if(!p.device)return p.allocated==0;
    if(p.retained||p.device!=dev.device)return false;const auto completed=vk_timeline_completed();bool released=true;
    if(p.probe_pending&&completed>=p.probe_recording){p.probe_pending=false;p.probe_source.reset();}
    if(p.probe_pending)released=false;else if(p.probe.buffer)gpu_scene_destroy(p,p.probe);
    for(auto& slot:p.slots){if(slot.pending&&completed>=slot.frame->recording)slot.pending=false;if(slot.pending){released=false;continue;}slot.frame.reset();if(slot.pin.use_count()>1){released=false;continue;}if(slot.storage.buffer)gpu_scene_destroy(p,slot.storage);}
    gpu_scene_report(p,released?"stopped":"stopped_resources_retained");return released;
}
}
