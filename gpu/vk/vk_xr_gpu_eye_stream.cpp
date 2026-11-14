// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_texture.h"
#include "vk_xr.h"
#include "xr_gpu_scene_source.h"
#include "xr_gpu_eye_stream.h"
#include "xr_depth_pair_reproject_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "vk_work.h"
namespace mkwvk {
struct XrGpuEyeFrameAccess {static void pin(XrGpuEyeFrame& frame,const std::shared_ptr<const void>& token){frame.lifetime_=token;}};
namespace {
using hookshot::vr::DepthPairParameters;using hookshot::vr::DepthPairDispatch;using hookshot::vr::DepthPairTrackingTarget;
int64_t eye_stream_now(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct EyeStreamBuffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;uint64_t address=0,bytes=0,allocated=0;void* mapped=nullptr;bool coherent=false;};
struct EyeStreamPin {};
struct EyeStreamSlot {EyeStreamBuffer storage;std::shared_ptr<const void> pin=std::make_shared<EyeStreamPin>();std::shared_ptr<XrGpuEyeFrame> frame;std::shared_ptr<const XrGpuSceneSource> source;bool pending=false;};
struct EyeStream {
    std::mutex mutex;VkDevice device=VK_NULL_HANDLE;uint64_t generation=0,origin=0,allocated=0,sequence=0,last_tick=UINT64_MAX,last_record_frame=UINT64_MAX;
    VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    PFN_vkUnmapMemory unmap=nullptr;PFN_vkInvalidateMappedMemoryRanges invalidate=nullptr;
    uint32_t width=0,height=0;std::array<EyeStreamSlot,3> slots;std::shared_ptr<const XrGpuEyeFrame> latest;
    uint64_t recorded=0,completed=0,published=0,leased_skips=0,pose_rejections=0,failures=0;bool stopped=false,retained=false;
    EyeStreamBuffer probe;std::shared_ptr<const XrGpuEyeFrame> probe_source;uint64_t probe_recording=0,probe_next=1;uint32_t probe_samples=0,probe_attempts=0;bool probe_pending=false;uint64_t last_eye_difference=0;uint32_t last_rgb_span=0;
};
EyeStream& eye_stream(){static auto* p=new EyeStream;return *p;}
bool eye_stream_probe(){return env_on("RX_VK_XR_GPU_EYE_STREAM_PROBE");}
void eye_stream_report(const EyeStream& p,const char* status){
    const char* path=std::getenv("HOOKSHOT_VR_GPU_EYE_STREAM_REPORT_FILE");if(!path||!*path)return;
    try{std::ofstream f(std::filesystem::u8path(path),std::ios::trunc);if(f)f<<"{\"schema\":1,\"status\":\""<<status<<"\",\"recorded_pairs\":"<<p.recorded<<",\"completed_pairs\":"<<p.completed<<",\"published_pairs\":"<<p.published<<",\"leased_skips\":"<<p.leased_skips<<",\"pose_rejections\":"<<p.pose_rejections<<",\"failures\":"<<p.failures<<",\"allocated_bytes\":"<<p.allocated<<",\"width_per_eye\":"<<p.width<<",\"height\":"<<p.height<<",\"probe_samples\":"<<p.probe_samples<<",\"probe_attempts\":"<<p.probe_attempts<<",\"last_eye_difference\":"<<p.last_eye_difference<<",\"last_rgb_span\":"<<p.last_rgb_span<<",\"source_CPU_upload\":false,\"intermediate_CPU_pixels\":false,\"output_validation_readback\":"<<(p.probe_attempts?"true":"false")<<",\"synthetic_tracking\":"<<(eye_stream_probe()?"true":"false")<<",\"XR_consumer_connected\":false,\"native_geometry_stereo\":false}\n";}catch(...){}
}
void eye_stream_free(EyeStream& p,EyeStreamBuffer& b){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);p.allocated-=b.allocated;b={};}
bool eye_stream_allocate(EyeStream& p,uint64_t bytes,bool host,EyeStreamBuffer& out){
    if(!bytes||bytes>16ull*1024*1024)return false;EyeStreamBuffer b;
    VkBufferCreateInfo create{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};create.size=bytes;create.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;if(!host)create.usage|=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    if(vkCreateBuffer(p.device,&create,nullptr,&b.buffer)!=VK_SUCCESS)return false;
    auto fail=[&](){if(b.mapped)p.unmap(p.device,b.memory);if(b.buffer)vkDestroyBuffer(p.device,b.buffer,nullptr);if(b.memory)vkFreeMemory(p.device,b.memory,nullptr);return false;};
    VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(p.device,b.buffer,&requirements);if(!requirements.size||requirements.size>16ull*1024*1024||requirements.size>64ull*1024*1024-p.allocated)return fail();
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(dev.physical,&memory);uint32_t type=UINT32_MAX;const auto wanted=host?VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT:VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&wanted)==wanted){type=i;break;}if(type==UINT32_MAX)return fail();b.coherent=(memory.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=host?nullptr:&flags;allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
    if(vkAllocateMemory(p.device,&allocation,nullptr,&b.memory)!=VK_SUCCESS||vkBindBufferMemory(p.device,b.buffer,b.memory,0)!=VK_SUCCESS)return fail();
    if(host){if(vkMapMemory(p.device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped)!=VK_SUCCESS||!b.mapped)return fail();}
    else{VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.buffer;b.address=vkGetBufferDeviceAddress(p.device,&info);if(!b.address||b.address%16||b.address>UINT64_MAX-bytes)return fail();}
    b.bytes=bytes;b.allocated=requirements.size;p.allocated+=b.allocated;out=b;return true;
}
uint32_t eye_stream_dimension(const char* name,uint32_t fallback,uint32_t maximum){const char* value=std::getenv(name);if(!value||!*value)return fallback;char* end=nullptr;const auto n=std::strtoul(value,&end,10);if(end==value||*end||n<8||n>maximum||n%8)return 0;return uint32_t(n);}
bool eye_stream_initialize(EyeStream& p,const XrGpuSceneSource& source){
    if(!dev.features.shaderInt64)return false;
    p.device=source.device;p.generation=source.generation;p.width=eye_stream_dimension("HOOKSHOT_VR_GPU_EYE_WIDTH",1024,1920);p.height=eye_stream_dimension("HOOKSHOT_VR_GPU_EYE_HEIGHT",1024,1080);if(!p.width||!p.height)return false;
    p.unmap=reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddr(p.device,"vkUnmapMemory"));p.invalidate=reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(vkGetDeviceProcAddr(p.device,"vkInvalidateMappedMemoryRanges"));if(!p.unmap||!p.invalidate)return false;
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kDepthPairReprojectSpv;shader.pCode=kDepthPairReprojectSpv;if(vkCreateShaderModule(p.device,&shader,nullptr,&p.shader)!=VK_SUCCESS)return false;
    VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(DepthPairDispatch)};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;if(vkCreatePipelineLayout(p.device,&layout,nullptr,&p.layout)!=VK_SUCCESS)return false;
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=p.layout;pipeline.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=p.shader;pipeline.stage.pName="main";
    return vkCreateComputePipelines(p.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&p.pipeline)==VK_SUCCESS;
}
void eye_stream_order(VkCommandBuffer command,VkPipelineStageFlags2 stage,VkAccessFlags2 access){VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstStageMask=stage;b.dstAccessMask=access;VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.memoryBarrierCount=1;d.pMemoryBarriers=&b;vkCmdPipelineBarrier2(command,&d);}
bool eye_stream_target(const XrGpuSceneSource& source,const XrFrameSnapshot& snapshot,bool probe,int64_t now,DepthPairTrackingTarget& target){
    target={};target.origin=source.pose.generation;target.observed_ns=probe?now:snapshot.observed_at_monotonic_ns;target.serial=probe?uint64_t(now):snapshot.sequence;
    if(probe){if(xr_vulkan_active()||!source.synthetic_pose||!env_on("HOOKSHOT_VR_IMMERSIVE_TEST"))return false;
        std::array<double,9> rotation;if(!hookshot::vr::depth_tracking_detail::rotation(source.pose.orientation,rotation))return false;
        for(unsigned eye=0;eye<2;++eye){auto& out=target.eyes[eye];std::copy(source.pose.orientation,source.pose.orientation+4,out.orientation.begin());const double side=eye?.032:-.032;for(unsigned r=0;r<3;++r)out.position[r]=float(source.pose.position[r]+side*rotation[3*r]);
            out.fov={-.9599311f,.9599311f,.8726646f,-.8726646f};out.orientation_valid=out.position_valid=true;}
    }else{if(!snapshot.running||!snapshot.focused||!snapshot.should_render||snapshot.origin_generation!=target.origin||(snapshot.head.flags&3)!=3)return false;
        for(unsigned eye=0;eye<2;++eye){auto& out=target.eyes[eye];const auto& in=snapshot.eyes[eye];std::copy(in.pose.orientation,in.pose.orientation+4,out.orientation.begin());std::copy(in.pose.position,in.pose.position+3,out.position.begin());std::copy(in.fov,in.fov+4,out.fov.begin());out.orientation_valid=(in.pose.flags&1)!=0;out.position_valid=(in.pose.flags&2)!=0;}}
    return true;
}
bool eye_stream_probe_write(EyeStream& p){
    const char* base=std::getenv("HOOKSHOT_VR_GPU_EYE_STREAM_IMAGE_BASE");if(!base||!*base||!p.probe_source)return false;const auto& frame=*p.probe_source;const uint64_t pixels=uint64_t(frame.width)*frame.height;const auto* rgba=static_cast<const unsigned char*>(p.probe.mapped);std::array<unsigned char,3> lo{255,255,255},hi{};p.last_eye_difference=0;
    for(uint64_t i=0;i<pixels;++i){if(std::memcmp(rgba+4*i,rgba+4*(pixels+i),3))++p.last_eye_difference;for(unsigned eye=0;eye<2;++eye)for(unsigned c=0;c<3;++c){const auto value=rgba[4*(eye*pixels+i)+c];lo[c]=std::min(lo[c],value);hi[c]=std::max(hi[c],value);}}
    p.last_rgb_span=0;for(unsigned c=0;c<3;++c)p.last_rgb_span=std::max(p.last_rgb_span,unsigned(hi[c]-lo[c]));if(p.last_rgb_span<32||p.last_eye_difference<std::max<uint64_t>(1,pixels/1000))return false;
    try{const auto path=std::filesystem::u8path(std::string(base)+"-"+std::to_string(p.probe_samples+1)+".ppm");if(std::filesystem::exists(path))return false;std::ofstream file(path,std::ios::binary|std::ios::trunc);file<<"P6\n"<<2*frame.width<<' '<<frame.height<<"\n255\n";std::vector<unsigned char> row(uint64_t(frame.width)*6);
        for(uint32_t y=0;y<frame.height;++y){for(unsigned eye=0;eye<2;++eye)for(uint32_t x=0;x<frame.width;++x)std::memcpy(row.data()+(uint64_t(eye)*frame.width+x)*3,rgba+eye*pixels*4+(uint64_t(y)*frame.width+x)*4,3);file.write(reinterpret_cast<const char*>(row.data()),std::streamsize(row.size()));}file.close();return bool(file);
    }catch(...){return false;}
}
}
bool xr_gpu_eye_stream_enabled(){static const bool enabled=env_on("RX_VK_XR_GPU_EYE_STREAM");return enabled;}
void xr_gpu_eye_stream_tick(uint64_t frame){
    if(!xr_gpu_eye_stream_enabled())return;auto& p=eye_stream();std::lock_guard lock(p.mutex);if(p.stopped||p.retained||p.last_tick==frame)return;p.last_tick=frame;
    if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();eye_stream_report(p,"device_changed_retained");return;}
    const auto completed=vk_timeline_completed();const auto before=p.completed;
    for(auto& slot:p.slots)if(slot.pending&&completed>=slot.frame->recording){slot.pending=false;slot.frame->ready_ns=eye_stream_now();slot.source.reset();++p.completed;if(slot.frame->target.origin==p.origin&&(!p.latest||slot.frame->state>p.latest->state)){p.latest=slot.frame;++p.published;}}
    if(p.probe_pending&&completed>=p.probe_recording){
        if(!p.probe.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=p.probe.memory;range.size=VK_WHOLE_SIZE;if(p.invalidate(p.device,1,&range)!=VK_SUCCESS){p.retained=true;eye_stream_report(p,"probe_invalidate_failed_retained");return;}}
        p.probe_pending=false;const bool written=eye_stream_probe_write(p);if(written)++p.probe_samples;p.probe_source.reset();p.probe_next=p.completed+(written?30:15);
        if(!written&&p.probe_attempts>=12){++p.failures;p.stopped=true;}eye_stream_report(p,written?"probe_sample_completed":p.stopped?"probe_attempt_limit":"probe_content_retry");
    }else if(p.completed!=before&&p.completed%30==0)eye_stream_report(p,"streaming");
}
void xr_gpu_eye_stream_network(VkCommandBuffer command,uint64_t recording,uint64_t program){
    if(!xr_gpu_eye_stream_enabled()||(program!=0xfd029a317a9a4b94ull&&program!=0x9dc98871eb7fc101ull)||!command||!recording||par_active||recording<=vk_timeline_completed())return;
    auto& p=eye_stream();std::lock_guard lock(p.mutex);if(p.stopped||p.retained)return;if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();eye_stream_report(p,"device_changed_retained");return;}
    const bool probe=eye_stream_probe();const auto now=eye_stream_now();XrFrameSnapshot snapshot{};if(!probe&&!xr_vulkan_latest_frame(&snapshot))return;
    const uint64_t origin=probe?1:snapshot.origin_generation;auto source=xr_gpu_scene_source_acquire(dev.device,origin,now,true,probe);
    if(source&&source->recording<recording&&source->queue_family==dev.queue_family&&p.last_record_frame!=texture_frame_no()){
        if(!p.device&&!eye_stream_initialize(p,*source)){++p.failures;p.stopped=true;eye_stream_report(p,"setup_failed");return;}
        if(source->generation!=p.generation){p.retained=true;p.latest.reset();eye_stream_report(p,"source_generation_changed_retained");return;}
        if(p.origin!=origin){p.origin=origin;p.latest.reset();}
        DepthPairTrackingTarget target;std::array<DepthPairParameters,2> cameras;
        hookshot::vr::DepthPairSource identity{source->address,source->bytes,source->state,origin,uint64_t(source->pose.observed_ns),source->recording,p.generation,source->width,source->height,source->format,source->queue_family,true,true};
        identity.projection={source->projection[0],source->projection[5],source->projection[10],source->projection[11]};identity.optical_center={source->projection[2],source->projection[6]};
        if(!eye_stream_target(*source,snapshot,probe,now,target)||!hookshot::vr::prepare_tracked_depth_pair_parameters(identity,source->pose,target,now,p.width,p.height,cameras)){++p.pose_rejections;return;}
        EyeStreamSlot* chosen=nullptr;for(auto& slot:p.slots){if(slot.pending)continue;slot.frame.reset();if(slot.pin.use_count()==1){chosen=&slot;break;}}
        if(!chosen){++p.leased_skips;return;}auto& slot=*chosen;const uint64_t eye_bytes=uint64_t(p.width)*p.height*4,pair_bytes=2*eye_bytes;
        if(!slot.storage.buffer&&!eye_stream_allocate(p,pair_bytes+256,false,slot.storage)){++p.failures;if(p.failures>=3)p.stopped=true;eye_stream_report(p,"slot_allocation_failed");return;}
        std::array<hookshot::vr::DepthPairOutput,2> outputs{};std::array<DepthPairDispatch,2> dispatches;
        for(unsigned eye=0;eye<2;++eye)outputs[eye]={slot.storage.address+eye*eye_bytes,eye_bytes,slot.storage.address+pair_bytes+eye*128,128,0,0,target.serial,origin,recording,p.generation,eye,source->queue_family};
        if(!hookshot::vr::prepare_depth_pair_dispatches(identity,recording,p.generation,outputs,cameras,dispatches)){++p.failures;p.stopped=true;eye_stream_report(p,"dispatch_scope_rejected");return;}
        try{slot.frame=std::make_shared<XrGpuEyeFrame>();}catch(...){++p.failures;eye_stream_report(p,"metadata_allocation_failed");return;}
        auto& out=*slot.frame;XrGpuEyeFrameAccess::pin(out,slot.pin);out.device=p.device;out.buffer=slot.storage.buffer;out.bytes=pair_bytes;out.state=++p.sequence;out.source_state=source->state;out.generation=p.generation;out.recording=recording;out.replay_frame=texture_frame_no();out.width=p.width;out.height=p.height;out.queue_family=dev.queue_family;out.source_observed_ns=source->pose.observed_ns;out.target=target;out.synthetic=probe||source->synthetic_pose;
        slot.source=std::move(source);eye_stream_order(command,VK_PIPELINE_STAGE_2_TRANSFER_BIT,VK_ACCESS_2_TRANSFER_WRITE_BIT);note_work();vkCmdUpdateBuffer(command,slot.storage.buffer,pair_bytes,sizeof cameras,cameras.data());
        eye_stream_order(command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,p.pipeline);
        for(const auto& dispatch:dispatches){vkCmdPushConstants(command,p.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof dispatch,&dispatch);vkCmdDispatch(command,(p.width+7)/8,(p.height+7)/8,1);}
        eye_stream_order(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT);slot.pending=true;p.last_record_frame=texture_frame_no();++p.recorded;if(p.recorded==1)eye_stream_report(p,"pair_recorded");
    }
    const char* image_base=std::getenv("HOOKSHOT_VR_GPU_EYE_STREAM_IMAGE_BASE");
    if(probe&&image_base&&*image_base&&p.latest&&!p.probe_pending&&p.probe_samples<3&&p.probe_attempts<12&&p.completed>=p.probe_next){
        if(!p.probe.buffer&&!eye_stream_allocate(p,p.latest->bytes,true,p.probe)){++p.failures;p.stopped=true;eye_stream_report(p,"probe_allocation_failed");return;}
        p.probe_source=p.latest;p.probe_recording=recording;++p.probe_attempts;eye_stream_order(command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferCopy copy{0,0,p.probe_source->bytes};vkCmdCopyBuffer(command,p.probe_source->buffer,p.probe.buffer,1,&copy);eye_stream_order(command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_HOST_READ_BIT);p.probe_pending=true;
    }
}
std::shared_ptr<const XrGpuEyeFrame> xr_gpu_eye_stream_acquire(VkDevice device,uint64_t origin,int64_t now,bool allow_synthetic){
    if(!xr_gpu_eye_stream_enabled()||!device||!origin||now<=0)return {};auto& p=eye_stream();std::lock_guard lock(p.mutex);
    if(p.device&&p.device!=dev.device){p.retained=true;p.latest.reset();return {};}const auto f=p.latest;
    if(p.stopped||p.retained||!f||f->device!=device||f->generation!=p.generation||f->target.origin!=origin||f->ready_ns<=0||now<f->ready_ns||now<f->source_observed_ns||now-f->source_observed_ns>250000000||(!allow_synthetic&&f->synthetic))return {};return f;
}
bool xr_gpu_eye_stream_stop(){
    if(!xr_gpu_eye_stream_enabled())return true;auto& p=eye_stream();std::lock_guard lock(p.mutex);p.stopped=true;p.latest.reset();if(!p.device)return true;
    if(p.retained||p.device!=dev.device){p.retained=true;return false;}const auto completed=vk_timeline_completed();
    for(auto& slot:p.slots)if(slot.pending){if(completed<slot.frame->recording)return false;slot.pending=false;slot.source.reset();}
    if(p.probe_pending){if(completed<p.probe_recording)return false;p.probe_pending=false;p.probe_source.reset();}
    for(auto& slot:p.slots)slot.frame.reset();for(const auto& slot:p.slots)if(slot.pin.use_count()>1)return false;
    for(auto& slot:p.slots)eye_stream_free(p,slot.storage);eye_stream_free(p,p.probe);if(p.pipeline)vkDestroyPipeline(p.device,p.pipeline,nullptr);if(p.layout)vkDestroyPipelineLayout(p.device,p.layout,nullptr);if(p.shader)vkDestroyShaderModule(p.device,p.shader,nullptr);p.pipeline=VK_NULL_HANDLE;p.layout=VK_NULL_HANDLE;p.shader=VK_NULL_HANDLE;eye_stream_report(p,"stopped");return true;
}
}
