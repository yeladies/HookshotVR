// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <chrono>
#include "xr_gpu_scene_source.h"
#include "xr_depth_pair_reproject_spv.h"
#include "xr_gpu_projection_layer.h"
namespace mkwvk { namespace {
#define LATE_PAIR_FUNCTIONS(X) \
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(GetBufferDeviceAddress) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreatePipelineLayout) X(DestroyPipelineLayout) X(CreateComputePipelines) X(DestroyPipeline) \
 X(CmdUpdateBuffer) X(CmdBindPipeline) X(CmdPushConstants) X(CmdDispatch) X(CmdPipelineBarrier)
struct GpuLatePair {
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    uint64_t address=0,bytes=0,allocated=0,generation=0,state=0;uint32_t width=0,height=0,family=0;float guard=1;bool fault=false;
    std::shared_ptr<const XrGpuSceneSource> held;
    std::array<hookshot::vr::DepthPairParameters,2> cameras{};hookshot::vr::DepthPairSource identity;hookshot::vr::GpuProjectionPair planned;
#define DECL(n) PFN_vk##n n=nullptr;
    LATE_PAIR_FUNCTIONS(DECL)
#undef DECL
    bool initialize(Bridge& bridge,VkInstance instance,VkPhysicalDevice physical,VkDevice d,uint32_t f,uint32_t w,uint32_t h){
        device=d;family=f;width=w;height=h;bytes=uint64_t(w)*h*8;
        if(!bridge.reproject_capable||bytes+256>64ull*1024*1024)return false;
        if(const char* value=std::getenv("HOOKSHOT_VR_GPU_EYE_TANGENT_SCALE")){char* end=nullptr;guard=std::strtof(value,&end);if(end==value||*end||!std::isfinite(guard)||guard<1||guard>1.25f)return false;}
        auto get=reinterpret_cast<PFN_vkGetDeviceProcAddr>(bridge.vulkan_get(instance,"vkGetDeviceProcAddr"));if(!get)return false;
#define LOAD(n) n=reinterpret_cast<PFN_vk##n>(get(device,"vk" #n));if(!n)return false;
        LATE_PAIR_FUNCTIONS(LOAD)
#undef LOAD
        auto properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(bridge.vulkan_get(instance,"vkGetPhysicalDeviceMemoryProperties"));if(!properties)return false;
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};ci.size=bytes+256;ci.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        if(CreateBuffer(device,&ci,nullptr,&buffer)!=VK_SUCCESS)return false;VkMemoryRequirements req{};GetBufferMemoryRequirements(device,buffer,&req);if(req.size<ci.size||req.size>64ull*1024*1024)return false;
        VkPhysicalDeviceMemoryProperties props{};properties(physical,&props);uint32_t type=UINT32_MAX;for(uint32_t i=0;i<props.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(props.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}if(type==UINT32_MAX)return false;
        VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mi.pNext=&flags;mi.allocationSize=req.size;mi.memoryTypeIndex=type;
        if(AllocateMemory(device,&mi,nullptr,&memory)!=VK_SUCCESS||BindBufferMemory(device,buffer,memory,0)!=VK_SUCCESS)return false;allocated=req.size;
        VkBufferDeviceAddressInfo ai{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};ai.buffer=buffer;address=GetBufferDeviceAddress(device,&ai);if(!address||address%16||address>UINT64_MAX-ci.size)return false;
        VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.codeSize=sizeof kDepthPairReprojectSpv;si.pCode=kDepthPairReprojectSpv;if(CreateShaderModule(device,&si,nullptr,&shader)!=VK_SUCCESS)return false;
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(hookshot::vr::DepthPairDispatch)};VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&range;if(CreatePipelineLayout(device,&li,nullptr,&layout)!=VK_SUCCESS)return false;
        VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pi.layout=layout;pi.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pi.stage.module=shader;pi.stage.pName="main";
        return CreateComputePipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&pipeline)==VK_SUCCESS;
    }
    bool prepare(const hookshot::vr::DepthPairTrackingTarget& target,int64_t now){
        auto source=xr_gpu_scene_source_acquire(device,target.origin,now);held.reset();
        // Acquisition may observe a newly completed packet. Validate against the
        // clock after that observation, not a timestamp taken before the query.
        now=std::max(now,std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
        if(!source||source->device!=device||!source->buffer||!source->address||source->address%16||source->queue_family!=family||!source->state||!source->recording||!source->generation||source->synthetic_pose||source->bytes!=uint64_t(source->width)*source->height*8||source->ready_ns<=0||source->ready_ns>now)return false;
        if(generation&&generation!=source->generation){fault=true;return false;}
        auto expanded=target;if(!hookshot::vr::expand_depth_pair_fovs(expanded,guard))return false;
        identity={source->address,source->bytes,source->state,target.origin,uint64_t(source->pose.observed_ns),source->recording,source->generation,source->width,source->height,source->format,family,true,true};
        identity.projection={source->projection[0],source->projection[5],source->projection[10],source->projection[11]};identity.optical_center={source->projection[2],source->projection[6]};
        if(!hookshot::vr::prepare_tracked_depth_pair_parameters(identity,source->pose,expanded,now,width,height,cameras))return false;
        for(auto& p:cameras){p.settings[2]=1;p.settings[3]=8;}
        generation=source->generation;planned={};planned.source_state=source->state;planned.device_generation=generation;planned.width=width;planned.height=height;planned.source_observed_ns=source->pose.observed_ns;planned.target=expanded;held=std::move(source);return true;
    }
    bool record(VkCommandBuffer command){
        if(!held||!pipeline)return false;std::array<hookshot::vr::DepthPairOutput,2> outputs{};std::array<hookshot::vr::DepthPairDispatch,2> dispatches{};const uint64_t recording=identity.recording;
        for(unsigned eye=0;eye<2;++eye)outputs[eye]={address+eye*(bytes/2),bytes/2,address+bytes+eye*128,128,0,0,planned.target.serial,planned.target.origin,recording,generation,eye,family};
        if(!hookshot::vr::prepare_depth_pair_dispatches(identity,recording,generation,outputs,cameras,dispatches))return false;
        VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&b,0,nullptr,0,nullptr);CmdUpdateBuffer(command,buffer,bytes,sizeof cameras,cameras.data());
        b.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;CmdPipelineBarrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&b,0,nullptr,0,nullptr);
        CmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);for(const auto& d:dispatches){CmdPushConstants(command,layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof d,&d);CmdDispatch(command,(width+7)/8,(height+7)/8,1);}++state;return true;
    }
    hookshot::vr::GpuProjectionPair completed(int64_t now)const{auto p=planned;p.ready_ns=now;p.immutable=p.complete=true;return p;}
    void discard(){held.reset();}
    void close(){held.reset();if(pipeline&&DestroyPipeline)DestroyPipeline(device,pipeline,nullptr);if(layout&&DestroyPipelineLayout)DestroyPipelineLayout(device,layout,nullptr);if(shader&&DestroyShaderModule)DestroyShaderModule(device,shader,nullptr);if(buffer&&DestroyBuffer)DestroyBuffer(device,buffer,nullptr);if(memory&&FreeMemory)FreeMemory(device,memory,nullptr);pipeline=VK_NULL_HANDLE;layout=VK_NULL_HANDLE;shader=VK_NULL_HANDLE;buffer=VK_NULL_HANDLE;memory=VK_NULL_HANDLE;}
};
#undef LATE_PAIR_FUNCTIONS
} }
