// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "vk_xr.h"
#include "xr_gpu_scene_source.h"
#include "xr_gpu_depth_pair_probe.h"
#include "xr_depth_pair_parameters.h"
#include "xr_depth_pair_reproject_spv.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "vk_work.h"
namespace mkwvk {
namespace {
using hookshot::vr::DepthPairParameters;
using hookshot::vr::DepthPairDispatch;
struct DepthProbeBuffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;uint64_t address=0,bytes=0,allocated=0;void* mapped=nullptr;bool coherent=false;};
struct DepthProbe {
    std::mutex mutex;VkDevice device=VK_NULL_HANDLE;uint64_t generation=0,allocated=0,last_tick=UINT64_MAX,recording=0,last_state=0;int64_t not_before_ns=0;uint32_t background_rejections=0;uint64_t different_eye_pixels=0;uint32_t rgb_span=0;
    VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    PFN_vkUnmapMemory unmap=nullptr;PFN_vkInvalidateMappedMemoryRanges invalidate=nullptr;
    DepthProbeBuffer output,parameters,counters,readback,debug_source;std::array<DepthPairParameters,2> last_parameters{};bool metadata_verified=false,debug_written=false;std::shared_ptr<const XrGpuSceneSource> source;
    uint32_t width=0,height=0,recorded=0,completed=0,failures=0;bool pending=false,stopped=false,retained=false,complete=false;
    std::array<uint64_t,3> source_states{},recordings{};std::array<uint32_t,8> counts{};
};
DepthProbe& depth_probe(){static auto* p=new DepthProbe;return *p;}
int64_t depth_probe_now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void depth_probe_report(const DepthProbe& p,const char* status){
    const char* path=std::getenv("HOOKSHOT_VR_GPU_DEPTH_PAIR_REPORT_FILE");if(!path||!*path)return;
    try{std::ofstream f(std::filesystem::u8path(path),std::ios::trunc);if(!f)return;
        f<<"{\"schema\":1,\"status\":\""<<status<<"\",\"recorded_pairs\":"<<p.recorded<<",\"completed_pairs\":"<<p.completed<<",\"background_rejections\":"<<p.background_rejections<<",\"different_eye_pixels\":"<<p.different_eye_pixels<<",\"rgb_span\":"<<p.rgb_span<<",\"content_verified\":"<<(p.complete?"true":"false")<<",\"failures\":"<<p.failures<<",\"complete\":"<<(p.complete?"true":"false")<<",\"allocated_bytes\":"<<p.allocated<<",\"source_CPU_validation_readback\":"<<(p.debug_source.buffer?"true":"false")<<",\"debug_source_written\":"<<(p.debug_written?"true":"false")<<",\"metadata_upload_verified\":"<<(p.metadata_verified?"true":"false")<<",\"width_per_eye\":"<<p.width<<",\"height\":"<<p.height<<",\"source_states\":[";
        for(unsigned i=0;i<3;++i)f<<(i?",":"")<<p.source_states[i];f<<"],\"recordings\":[";for(unsigned i=0;i<3;++i)f<<(i?",":"")<<p.recordings[i];
        f<<"],\"last_counts\":[";for(unsigned i=0;i<8;++i)f<<(i?",":"")<<p.counts[i];
        f<<"],\"same_GPU_source_per_pair\":true,\"source_CPU_upload\":false,\"intermediate_CPU_pixels\":false,\"validation_output_readback\":true,\"XR\":false,\"native_geometry_stereo\":false,\"scale_calibrated\":false}\n";
    }catch(...){}
}
void depth_probe_free(DepthProbe& p,DepthProbeBuffer& b){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);p.allocated-=b.allocated;b={};}
void depth_probe_release(DepthProbe& p){for(auto* b:{&p.output,&p.parameters,&p.counters,&p.readback,&p.debug_source})depth_probe_free(p,*b);if(p.pipeline)vkDestroyPipeline(p.device,p.pipeline,nullptr);if(p.layout)vkDestroyPipelineLayout(p.device,p.layout,nullptr);if(p.shader)vkDestroyShaderModule(p.device,p.shader,nullptr);p.pipeline=VK_NULL_HANDLE;p.layout=VK_NULL_HANDLE;p.shader=VK_NULL_HANDLE;}
bool depth_probe_allocate(DepthProbe& p,uint64_t bytes,bool host,DepthProbeBuffer& out){
    if(!bytes||bytes>16ull*1024*1024)return false;DepthProbeBuffer b;
    VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};create.size=bytes;create.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if(!host)create.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if(vkCreateBuffer(p.device,&create,nullptr,&b.buffer)!=VK_SUCCESS)return false;
    auto fail=[&](){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);return false;};
    VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(p.device,b.buffer,&requirements);
    if(!requirements.size||requirements.size>16ull*1024*1024||requirements.size>40ull*1024*1024-p.allocated)return fail();
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(dev.physical,&memory);const auto wanted=host?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&wanted)==wanted){type=i;break;}if(type==UINT32_MAX)return fail();
    b.coherent=(memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=host?nullptr:&flags;allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
    if(vkAllocateMemory(p.device,&allocation,nullptr,&b.memory)!=VK_SUCCESS||vkBindBufferMemory(p.device,b.buffer,b.memory,0)!=VK_SUCCESS)return fail();
    if(host){if(vkMapMemory(p.device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped)!=VK_SUCCESS||!b.mapped)return fail();}
    else {VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.buffer;b.address=vkGetBufferDeviceAddress(p.device,&info);if(!b.address||b.address%16||b.address>UINT64_MAX-bytes)return fail();}
    b.bytes=bytes;b.allocated=requirements.size;p.allocated+=b.allocated;out=b;return true;
}
bool depth_probe_create(DepthProbe& p,const XrGpuSceneSource& source){
    p.device=source.device;p.generation=source.generation;p.width=source.width;p.height=source.height;
    p.unmap=reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddr(p.device,"vkUnmapMemory"));p.invalidate=reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(vkGetDeviceProcAddr(p.device,"vkInvalidateMappedMemoryRanges"));if(!p.unmap||!p.invalidate)return false;
    const uint64_t pair_bytes=uint64_t(p.width)*p.height*8;
    if(!depth_probe_allocate(p,pair_bytes,false,p.output)||!depth_probe_allocate(p,256,false,p.parameters)||!depth_probe_allocate(p,32,false,p.counters)||!depth_probe_allocate(p,pair_bytes+32,true,p.readback))return false;
    const char* debug_path=std::getenv("HOOKSHOT_VR_GPU_DEPTH_PAIR_SOURCE_DEBUG_FILE");if(debug_path&&*debug_path&&!depth_probe_allocate(p,source.bytes+256,true,p.debug_source))return false;
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kDepthPairReprojectSpv;shader.pCode=kDepthPairReprojectSpv;
    if(vkCreateShaderModule(p.device,&shader,nullptr,&p.shader)!=VK_SUCCESS)return false;
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(DepthPairDispatch)};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    if(vkCreatePipelineLayout(p.device,&layout,nullptr,&p.layout)!=VK_SUCCESS)return false;
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=p.layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=p.shader;pipeline.stage.pName="main";
    return vkCreateComputePipelines(p.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&p.pipeline)==VK_SUCCESS;
}
void depth_probe_order(VkCommandBuffer command,VkPipelineStageFlags2 stage,VkAccessFlags2 access){VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstStageMask=stage;b.dstAccessMask=access;VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.memoryBarrierCount=1;d.pMemoryBarriers=&b;vkCmdPipelineBarrier2(command,&d);}
bool depth_probe_write(const DepthProbe& p){
    const char* path=std::getenv("HOOKSHOT_VR_GPU_DEPTH_PAIR_IMAGE_FILE");if(!path||!*path||!p.readback.mapped)return false;
    try{std::ofstream f(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);f<<"P6\n"<<2*p.width<<" "<<p.height<<"\n255\n";
        const auto* data=static_cast<const unsigned char*>(p.readback.mapped);const uint64_t eye_bytes=uint64_t(p.width)*p.height*4;std::vector<unsigned char> row(uint64_t(p.width)*6);
        for(uint32_t y=0;y<p.height;++y){for(unsigned eye=0;eye<2;++eye)for(uint32_t x=0;x<p.width;++x)std::memcpy(row.data()+(uint64_t(eye)*p.width+x)*3,data+eye*eye_bytes+(uint64_t(y)*p.width+x)*4,3);f.write(reinterpret_cast<const char*>(row.data()),std::streamsize(row.size()));}f.close();return bool(f);
    }catch(...){return false;}
}
bool depth_probe_write_source(DepthProbe& p){
    if(!p.debug_source.buffer)return true;const char* path=std::getenv("HOOKSHOT_VR_GPU_DEPTH_PAIR_SOURCE_DEBUG_FILE");if(!path||!*path||!p.source)return false;
    if(!p.debug_source.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=p.debug_source.memory;range.size=VK_WHOLE_SIZE;if(p.invalidate(p.device,1,&range)!=VK_SUCCESS)return false;}
    const auto& f=*p.source;p.metadata_verified=std::memcmp(static_cast<const unsigned char*>(p.debug_source.mapped)+f.bytes,p.last_parameters.data(),256)==0;if(!p.metadata_verified)return false;
    try{std::ofstream file(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);const char magic[8]={'H','S','V','R','D','2',0,0};file.write(magic,8);file.write(reinterpret_cast<const char*>(&f.width),4);file.write(reinterpret_cast<const char*>(&f.height),4);file.write(reinterpret_cast<const char*>(&f.format),4);file.write(reinterpret_cast<const char*>(f.projection.data()),64);file.write(reinterpret_cast<const char*>(f.view.data()),48);file.write(static_cast<const char*>(p.debug_source.mapped),std::streamsize(f.bytes));file.close();return bool(file);}catch(...){return false;}
}
}
bool xr_gpu_depth_pair_probe_enabled(){static const bool enabled=env_on("RX_VK_XR_GPU_DEPTH_PAIR_PROBE");return enabled;}
void xr_gpu_depth_pair_probe_tick(uint64_t frame){
    if(!xr_gpu_depth_pair_probe_enabled())return;auto& p=depth_probe();std::lock_guard lock(p.mutex);if(p.stopped||p.retained||p.complete||p.last_tick==frame)return;p.last_tick=frame;
    if(p.device&&p.device!=dev.device){p.retained=true;depth_probe_report(p,"device_changed_retained");return;}
    if(!p.pending||vk_timeline_completed()<p.recording)return;
    if(!p.readback.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=p.readback.memory;range.size=VK_WHOLE_SIZE;if(p.invalidate(p.device,1,&range)!=VK_SUCCESS){p.retained=true;depth_probe_report(p,"invalidate_failed_retained");return;}}
    const uint64_t pixels=uint64_t(p.width)*p.height;std::memcpy(p.counts.data(),static_cast<const unsigned char*>(p.readback.mapped)+pixels*8,32);
    p.pending=false;
    for(unsigned eye=0;eye<2;++eye){uint64_t total=0;for(unsigned i=0;i<4;++i)total+=p.counts[eye*4+i];if(total!=pixels||!p.counts[eye*4]){p.source.reset();++p.failures;p.stopped=true;depth_probe_report(p,"counter_verification_failed");return;}}
    const auto* rgba=static_cast<const unsigned char*>(p.readback.mapped);std::array<unsigned char,3> lo{255,255,255},hi{};p.different_eye_pixels=0;
    for(uint64_t i=0;i<pixels;++i){if(std::memcmp(rgba+4*i,rgba+4*(pixels+i),3))++p.different_eye_pixels;for(unsigned eye=0;eye<2;++eye)for(unsigned c=0;c<3;++c){const auto value=rgba[4*(eye*pixels+i)+c];lo[c]=std::min(lo[c],value);hi[c]=std::max(hi[c],value);}}
    p.rgb_span=0;for(unsigned c=0;c<3;++c)p.rgb_span=std::max(p.rgb_span,unsigned(hi[c]-lo[c]));
    // A transport/count success alone can describe a uniform background. This
    // bounded diagnostic needs visible content and distinct eyes as well. It is
    // not a general scene-validity test or a production rendering heuristic.
    if(p.rgb_span<32||p.different_eye_pixels<std::max<uint64_t>(1,pixels/1000)){p.source.reset();++p.background_rejections;p.not_before_ns=depth_probe_now()+250000000;if(p.recorded>=12){p.stopped=true;++p.failures;}depth_probe_report(p,p.stopped?"background_attempt_limit":"waiting_for_visible_content");return;}
    if(p.completed==2&&p.debug_source.buffer){p.debug_written=depth_probe_write_source(p);if(!p.debug_written){++p.failures;p.stopped=true;depth_probe_report(p,"debug_source_failed");p.source.reset();return;}}p.source.reset();
    ++p.completed;if(p.completed==3){p.complete=depth_probe_write(p);if(!p.complete){++p.failures;p.stopped=true;}depth_probe_report(p,p.complete?"completed":"image_write_failed");}else depth_probe_report(p,"pair_completed");
}
void xr_gpu_depth_pair_probe_network(VkCommandBuffer command,uint64_t recording,uint64_t program){
    if(!xr_gpu_depth_pair_probe_enabled()||(program!=0xfd029a317a9a4b94ull&&program!=0x9dc98871eb7fc101ull)||!command||!recording||par_active)return;
    auto& p=depth_probe();std::lock_guard lock(p.mutex);if(p.stopped||p.retained||p.pending||p.complete||p.recorded>=12||depth_probe_now()<p.not_before_ns||recording<=vk_timeline_completed())return;
    if(p.device&&p.device!=dev.device){p.retained=true;depth_probe_report(p,"device_changed_retained");return;}
    auto source=xr_gpu_scene_source_acquire(dev.device,0,depth_probe_now(),false);
    if(!source||source->recording>=recording||source->state<=p.last_state||source->queue_family!=dev.queue_family)return;
    if(!p.device&&!depth_probe_create(p,*source)){++p.failures;p.stopped=true;depth_probe_release(p);depth_probe_report(p,"setup_failed");return;}
    if(source->generation!=p.generation||source->width!=p.width||source->height!=p.height){p.stopped=true;++p.failures;depth_probe_report(p,"source_scope_changed");return;}
    const uint64_t eye_bytes=uint64_t(p.width)*p.height*4;
    hookshot::vr::DepthPairSource identity{source->address,source->bytes,source->state,1,source->state,source->recording,source->generation,source->width,source->height,source->format,source->queue_family,true,true};
    identity.projection={source->projection[0],source->projection[5],source->projection[10],source->projection[11]};identity.optical_center={source->projection[2],source->projection[6]};
    std::array<DepthPairParameters,2> cameras{};std::array<hookshot::vr::DepthPairOutput,2> eyes{};std::array<DepthPairDispatch,2> dispatches{};
    for(unsigned eye=0;eye<2;++eye){auto& camera=cameras[eye];camera.source_from_eye={1,0,0,eye?.032f:-.032f,0,1,0,0,0,0,1,0};camera.eye_tangents={(-1+identity.optical_center[0])/identity.projection[0],(1+identity.optical_center[0])/identity.projection[0],(1+identity.optical_center[1])/identity.projection[1],(-1+identity.optical_center[1])/identity.projection[1]};camera.projection=identity.projection;camera.optical={identity.optical_center[0],identity.optical_center[1],25000,.85f};camera.dimensions={p.width,p.height,p.width,p.height};camera.settings={source->format,8,0,0};
        eyes[eye]={p.output.address+eye*eye_bytes,eye_bytes,p.parameters.address+eye*128,128,p.counters.address+eye*16,16,source->state,1,recording,p.generation,eye,source->queue_family};}
    if(!hookshot::vr::prepare_depth_pair_dispatches(identity,recording,p.generation,eyes,cameras,dispatches)){++p.failures;p.stopped=true;depth_probe_report(p,"dispatch_scope_rejected");return;}
    // This callback precedes the original upscaler's compute binds. No source
    // descriptors are rebound and both eyes use the same retained source lease.
    p.last_parameters=cameras;p.source=std::move(source);p.recording=recording;p.last_state=p.source->state;
    depth_probe_order(command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);
    note_work();vkCmdUpdateBuffer(command,p.parameters.buffer,0,sizeof cameras,cameras.data());note_work();vkCmdFillBuffer(command,p.counters.buffer,0,32,0);
    depth_probe_order(command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
    for(const auto& dispatch:dispatches){vkCmdPushConstants(command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof dispatch,&dispatch);vkCmdDispatch(command,(p.width+7)/8,(p.height+7)/8,1);}
    depth_probe_order(command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    const VkBufferCopy pixels{0,0,eye_bytes*2},counts{0,eye_bytes*2,32};vkCmdCopyBuffer(command,p.output.buffer,p.readback.buffer,1,&pixels);vkCmdCopyBuffer(command,p.counters.buffer,p.readback.buffer,1,&counts);
    if(p.debug_source.buffer){const VkBufferCopy source_copy{0,0,p.source->bytes},meta_copy{0,p.source->bytes,256};vkCmdCopyBuffer(command,p.source->buffer,p.debug_source.buffer,1,&source_copy);vkCmdCopyBuffer(command,p.parameters.buffer,p.debug_source.buffer,1,&meta_copy);}
    depth_probe_order(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);
    p.pending=true;p.source_states[p.completed]=p.last_state;p.recordings[p.completed]=recording;++p.recorded;depth_probe_report(p,"pair_recorded");
}
bool xr_gpu_depth_pair_probe_stop(){
    if(!xr_gpu_depth_pair_probe_enabled())return true;auto& p=depth_probe();std::lock_guard lock(p.mutex);p.stopped=true;if(!p.device)return true;
    if(p.retained||p.device!=dev.device){p.retained=true;return false;}if(p.pending&&vk_timeline_completed()<p.recording)return false;
    p.pending=false;p.source.reset();depth_probe_release(p);return true;
}
}
