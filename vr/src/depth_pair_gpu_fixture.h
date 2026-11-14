// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Offscreen verification fixture. It uploads authored/private file input and
// reads completed output; it is not the native live-game source owner.
#include "gpu_smoke_context.h"
#include "depth_pair_reproject.h"
#include "depth_pair_reproject_spv.h"
#include "same_state_gpu_copy.h"
#include <algorithm>
namespace hookshot::vr::smoke {
struct FixturePairDelete {Context* context;void operator()(EyeResourcePair* p)const{if(!context->uncertain)delete p;}};
struct DepthPairReadback {std::vector<uint32_t> pixels,source;std::array<std::array<uint32_t,4>,2> counters{};};
class DepthPairGpuFixture {
public:
    Context c;SmokeDevice gpu;uint32_t sw,sh,tw,th;uint64_t source_bytes,eye_bytes,pair_bytes,readback_bytes,recording=0;
    std::unique_ptr<EyeResourcePair,FixturePairDelete> eyes,target;
    VkCommandBuffer command=VK_NULL_HANDLE;PFN_vkCmdPipelineBarrier barrier=nullptr;bool coherent=false;
    DepthPairGpuFixture(uint32_t source_width,uint32_t source_height,uint32_t target_width,uint32_t target_height):gpu(initialize_smoke(c)),sw(source_width),sh(source_height),tw(target_width),th(target_height),
        source_bytes(uint64_t(sw)*sh*8),eye_bytes(uint64_t(tw)*th*4),pair_bytes(2*eye_bytes),readback_bytes(pair_bytes+source_bytes+32),eyes(new EyeResourcePair,FixturePairDelete{&c}),target(new EyeResourcePair,FixturePairDelete{&c}){
    require(sw>=2&&sh>=2&&sw<=1920&&sh<=1080&&tw>=2&&th>=2&&tw<=1920&&th<=1080,"bounded source/target fixture dimensions");
    const auto api=gpu.api;
    require(eyes->create(c.device,api,gpu.memory,tw,th,VK_FORMAT_R8G8B8A8_SRGB,16,64*1024*1024),"allocate independent eye images");
    require(target->create(c.device,api,gpu.memory,2*tw,th,VK_FORMAT_R8G8B8A8_SRGB,16,64*1024*1024),"allocate same-state pair target");
    address_buffer(c,api,gpu.memory,source_bytes,c.auxiliary[0]);address_buffer(c,api,gpu.memory,256,c.auxiliary[1]);address_buffer(c,api,gpu.memory,pair_bytes,c.auxiliary[2]);address_buffer(c,api,gpu.memory,32,c.auxiliary[3]);
    VkBufferCreateInfo staging{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};staging.size=readback_bytes;staging.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    checked(api.create_buffer(c.device,&staging,nullptr,&c.staging),"create bounded readback");VkMemoryRequirements requirements{};api.buffer_requirements(c.device,c.staging,&requirements);require(requirements.size<=64*1024*1024,"readback allocation bound");
    uint32_t type=UINT32_MAX;for(uint32_t i=0;i<gpu.memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){type=i;coherent=(gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;break;}
    require(type!=UINT32_MAX,"readback host visibility");VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;checked(api.allocate(c.device,&allocation,nullptr,&c.memory),"allocate readback");checked(api.bind_buffer(c.device,c.staging,c.memory,0),"bind readback");
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kDepthPairReprojectSpv;shader.pCode=kDepthPairReprojectSpv;
    checked(c.fn<PFN_vkCreateShaderModule>("vkCreateShaderModule")(c.device,&shader,nullptr,&c.shader),"create authored depth shader");VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(DepthPairDispatch)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;checked(c.fn<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(c.device,&layout,nullptr,&c.layout),"create depth layout");
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=c.layout;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=c.shader;pipeline.stage.pName="main";
    checked(c.fn<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(c.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&c.pipeline),"create depth pipeline");
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=gpu.family;checked(c.fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(c.device,&pool,nullptr,&c.pool),"create compute pool");
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commands.commandPool=c.pool;commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=1;checked(c.fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(c.device,&commands,&command),"allocate command");
    barrier=c.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
    }
    void order(VkPipelineStageFlags source,VkPipelineStageFlags destination,VkAccessFlags read,VkAccessFlags write){VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=read;b.dstAccessMask=write;barrier(command,source,destination,0,1,&b,0,nullptr,0,nullptr);}
    DepthPairReadback run(const std::vector<uint32_t>& source,const std::array<DepthPairParameters,2>& cameras,uint64_t state){
        require(state&&source.size()*4==source_bytes&&!c.uncertain,"complete source packet and known fixture ownership");
        if(recording){require(eyes->rearm(),"rearm only completed work");checked(c.fn<PFN_vkResetCommandPool>("vkResetCommandPool")(c.device,c.pool,0),"reuse completed command pool");}
        ++recording;
        DepthPairSource identity{c.auxiliary[0].address,source_bytes,state,1,1,recording,1,sw,sh,cameras[0].settings[0],gpu.family,true,true};
        identity.projection=cameras[0].projection;identity.optical_center={cameras[0].optical[0],cameras[0].optical[1]};
        std::array<DepthPairOutput,2> destinations;for(unsigned eye=0;eye<2;++eye)destinations[eye]={c.auxiliary[2].address+eye*eye_bytes,eye_bytes,c.auxiliary[1].address+eye*128,128,c.auxiliary[3].address+eye*16,16,recording,1,recording,1,eye,gpu.family};
        std::array<DepthPairDispatch,2> dispatches;require(prepare_depth_pair_dispatches(identity,recording,1,destinations,cameras,dispatches),"admit complete single-source pair");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(c.fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(command,&begin),"begin GPU pair");
        for(uint64_t offset=0;offset<source_bytes;offset+=65536){const uint64_t bytes=std::min<uint64_t>(65536,source_bytes-offset);c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[0].buffer,offset,bytes,reinterpret_cast<const unsigned char*>(source.data())+offset);}
        c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[1].buffer,0,sizeof cameras,cameras.data());
        c.fn<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(command,c.auxiliary[3].buffer,0,32,0);
        order(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        c.fn<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(command,VK_PIPELINE_BIND_POINT_COMPUTE,c.pipeline);
        for(unsigned eye=0;eye<2;++eye){const auto& dispatch=dispatches[eye];
            require(depth_pair_dispatch_valid(dispatch,cameras[eye],source_bytes,eye_bytes,128,16),"admit bounded source/output dispatch");
            c.fn<PFN_vkCmdPushConstants>("vkCmdPushConstants")(command,c.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof dispatch,&dispatch);c.fn<PFN_vkCmdDispatch>("vkCmdDispatch")(command,(tw+7)/8,(th+7)/8,1);
        }
        order(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        std::array<VkImageMemoryBarrier,2> image_barriers{};
        for(unsigned eye=0;eye<2;++eye){auto& b=image_barriers[eye];b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.image=eyes->eye(eye)->image;b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;}
        barrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,image_barriers.data());
        for(unsigned eye=0;eye<2;++eye){VkBufferImageCopy copy{};copy.bufferOffset=eye*eye_bytes;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={tw,th,1};c.fn<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(command,c.auxiliary[2].buffer,eyes->eye(eye)->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            image_barriers[eye].oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image_barriers[eye].newLayout=VK_IMAGE_LAYOUT_GENERAL;image_barriers[eye].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image_barriers[eye].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;}
        barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,image_barriers.data());
        std::array<SameStateEyeImage,2> pair;for(unsigned eye=0;eye<2;++eye)pair[eye]={eyes->eye(eye)->image,state,1,recording,recording,eye,tw,th,gpu.family,VK_FORMAT_R8G8B8A8_SRGB};
        require(record_same_state_eye_copy(command,recording,pair,{target->eye(0)->image,2*tw,th,gpu.family,VK_FORMAT_R8G8B8A8_SRGB,true,true},barrier,c.fn<PFN_vkCmdCopyImage>("vkCmdCopyImage")),"copy complete same-source eye pair without intermediate CPU pixels");
        VkImageMemoryBarrier read{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};read.srcQueueFamilyIndex=read.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;read.image=target->eye(0)->image;read.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};read.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;read.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;read.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;read.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&read);
        VkBufferImageCopy image_copy{};image_copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};image_copy.imageExtent={2*tw,th,1};c.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,target->eye(0)->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.staging,1,&image_copy);
        VkBufferCopy source_copy{0,pair_bytes,source_bytes},counter_copy{0,pair_bytes+source_bytes,32};c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,c.auxiliary[0].buffer,c.staging,1,&source_copy);c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,c.auxiliary[3].buffer,c.staging,1,&counter_copy);
        order(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT);checked(c.fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command),"end GPU pair");
        const auto submitted=eyes->submit(c.queue,command);c.uncertain=eyes->pending();checked(submitted,"submit same-source pair");require(eyes->poll(5000000000ull),"GPU pair timeout; allocations retained");c.uncertain=false;
        void* mapped=nullptr;checked(c.fn<PFN_vkMapMemory>("vkMapMemory")(c.device,c.memory,0,VK_WHOLE_SIZE,0,&mapped),"map completed output");if(!coherent){VkMappedMemoryRange invalidate{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};invalidate.memory=c.memory;invalidate.offset=0;invalidate.size=VK_WHOLE_SIZE;checked(c.fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(c.device,1,&invalidate),"invalidate completed output");}

        DepthPairReadback out;out.pixels.resize(pair_bytes/4);out.source.resize(source_bytes/4);
        std::memcpy(out.pixels.data(),mapped,pair_bytes);std::memcpy(out.source.data(),static_cast<const unsigned char*>(mapped)+pair_bytes,source_bytes);
        std::memcpy(out.counters.data(),static_cast<const unsigned char*>(mapped)+pair_bytes+source_bytes,32);
        c.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(c.device,c.memory);return out;
    }
};
}
