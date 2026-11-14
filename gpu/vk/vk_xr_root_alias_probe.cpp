// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_internal.h"
#include "vk_draw.h"
#include "xr_root_alias_probe.h"
#include "xr_root_alias_parameters.h"
#include "xr_root_alias_patch_spv.h"
#include <memory>
#include <cstring>
#include <new>
#include "vk_work.h"
namespace mkwvk {
struct XrRootAliasProbe {
    struct Buffer {VkBuffer buffer=VK_NULL_HANDLE;VmaAllocation memory=nullptr;uint64_t address=0;};
    VkDevice device=VK_NULL_HANDLE;XrRootAliasProbeInput input;XrUniformCopySource source;XrGuestRootSource root;
    uint64_t page_begin=0,page_bytes=0,inner=0,stride=0,readback_bytes=0,allocated=0,source_page_address=0;
    std::array<Buffer,2> pages{},roots{};Buffer readback;void* mapped=nullptr;
    VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    bool recorded=false,retained=false;
};
namespace {
constexpr uint64_t root_probe_budget=96ull*1024*1024,root_probe_entries=3145728;
bool root_probe_stamp(const XrUniformSourceStamp& a,const XrUniformSourceStamp& b){return a.recording&&a.pool_generation&&a.capture_generation&&a.recording==b.recording&&a.pool_generation==b.pool_generation&&a.capture_generation==b.capture_generation;}
void root_probe_dispose(XrRootAliasProbe* p){
    for(auto& b:p->pages)if(b.buffer)destroy_buffer(b.buffer,b.memory);
    for(auto& b:p->roots)if(b.buffer)destroy_buffer(b.buffer,b.memory);
    if(p->readback.buffer)destroy_buffer(p->readback.buffer,p->readback.memory);
    if(p->pipeline)vkDestroyPipeline(p->device,p->pipeline,nullptr);
    if(p->layout)vkDestroyPipelineLayout(p->device,p->layout,nullptr);
    if(p->shader)vkDestroyShaderModule(p->device,p->shader,nullptr);delete p;
}
bool root_probe_account(XrRootAliasProbe& p,VmaAllocation memory){const auto n=allocation_size(memory);if(!n||n>root_probe_budget-p.allocated)return false;p.allocated+=n;return true;}
bool root_probe_address(XrRootAliasProbe& p,XrRootAliasProbe::Buffer& b,uint64_t bytes){
    VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=b.buffer;b.address=vkGetBufferDeviceAddress(p.device,&info);
    return b.address&&b.address%16==0&&b.address<=UINT64_MAX-bytes;
}
void root_probe_order(VkCommandBuffer command,VkPipelineStageFlags2 destination,VkAccessFlags2 access){
    VkMemoryBarrier2 b{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};b.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;b.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstStageMask=destination;b.dstAccessMask=access;
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};d.memoryBarrierCount=1;d.pMemoryBarriers=&b;vkCmdPipelineBarrier2(command,&d);
}
XrRootAliasProbe* root_probe_create(const XrRootAliasProbeInput& in){
    if(!in.command||!in.recording||vk_timeline_completed()>=in.recording||!in.guest_camera||in.guest_camera%4||!in.camera.buffer||in.camera.offset%16||in.camera.range<0x230||in.camera.range>65536||in.camera.range%4||
       !in.matrices[0]||!in.matrices[1]||in.matrices[0]==in.matrices[1]||in.matrices[0]==in.camera.buffer||in.matrices[1]==in.camera.buffer||
       (in.guest_camera&4095)!=(in.camera.offset&4095)||in.camera.offset>UINT64_MAX-in.camera.range-4095)return nullptr;
    std::unique_ptr<XrRootAliasProbe,decltype(&root_probe_dispose)> owned(new(std::nothrow) XrRootAliasProbe,root_probe_dispose);
    auto* p=owned.get();if(!p)return nullptr;p->device=dev.device;p->input=in;
    if(!memory_xr_uniform_copy_source(in.camera.buffer,in.recording,p->source)||!memory_xr_guest_root_source(in.recording,p->root)||
       !root_probe_stamp(p->source.stamp,p->root.stamp)||p->root.bytes!=root_probe_entries*8||!(p->source.usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))return nullptr;
    p->page_begin=in.camera.offset&~4095ull;p->inner=in.camera.offset-p->page_begin;p->page_bytes=((in.camera.offset+in.camera.range+4095)&~4095ull)-p->page_begin;
    if(!xr_uniform_copy_source_valid(p->source,in.camera.buffer,p->page_begin,p->page_bytes,p->source.stamp))return nullptr;
    VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};address.buffer=in.camera.buffer;
    const auto base=vkGetBufferDeviceAddress(p->device,&address);
    if(!base||base>UINT64_MAX-p->source.bytes)return nullptr;p->source_page_address=base+p->page_begin;
    const auto usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    for(unsigned eye=0;eye<2;++eye){
        auto& page=p->pages[eye];auto& root=p->roots[eye];
        if(!create_device_buffer(p->page_bytes,usage,page.buffer,page.memory,true)||!root_probe_account(*p,page.memory)||!root_probe_address(*p,page,p->page_bytes)||
           !create_device_buffer(p->root.bytes,usage,root.buffer,root.memory,true)||!root_probe_account(*p,root.memory)||!root_probe_address(*p,root,p->root.bytes))return nullptr;
    }
    p->stride=(in.camera.range+15)&~15ull;p->readback_bytes=2*p->stride+32;
    if(!create_buffer(p->readback_bytes,usage,true,p->readback.buffer,p->readback.memory,&p->mapped,true)||!p->mapped||!root_probe_account(*p,p->readback.memory)||!root_probe_address(*p,p->readback,p->readback_bytes))return nullptr;
    for(unsigned eye=0;eye<2;++eye){hookshot::vr::EyeRootAliasParameters q{p->root.address,p->roots[eye].address,p->source_page_address,p->pages[eye].address,p->readback.address+2*p->stride+eye*16,uint32_t(root_probe_entries),uint32_t(p->page_bytes),0,0};
        if(!hookshot::vr::eye_root_alias_parameters_valid(q,p->root.bytes,p->root.bytes,p->page_bytes,p->page_bytes,16))return nullptr;
        q={p->roots[eye].address,p->readback.address+eye*p->stride,in.guest_camera,p->pages[eye].address,p->readback.address+2*p->stride+eye*16,uint32_t(root_probe_entries),uint32_t(p->page_bytes),2,uint32_t(in.camera.range)};
        if(!hookshot::vr::eye_root_lookup_parameters_valid(q,p->root.bytes,in.camera.range,p->page_bytes,16))return nullptr;
    }
    // Also reject cross-eye overlap, beyond the per-dispatch validation above.
    for(unsigned a=0;a<2;++a)for(unsigned b=0;b<2;++b){
        const auto ra=p->roots[a].address,pb=p->pages[b].address;
        if(ra<pb+p->page_bytes&&pb<ra+p->root.bytes)return nullptr;
    }
    if(p->roots[0].address<p->roots[1].address+p->root.bytes&&p->roots[1].address<p->roots[0].address+p->root.bytes)return nullptr;
    if(p->pages[0].address<p->pages[1].address+p->page_bytes&&p->pages[1].address<p->pages[0].address+p->page_bytes)return nullptr;
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kEyeRootAliasPatchSpv;shader.pCode=kEyeRootAliasPatchSpv;
    if(vkCreateShaderModule(p->device,&shader,nullptr,&p->shader)!=VK_SUCCESS)return nullptr;
    VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(hookshot::vr::EyeRootAliasParameters)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
    if(vkCreatePipelineLayout(p->device,&layout,nullptr,&p->layout)!=VK_SUCCESS)return nullptr;
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=p->layout;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=p->shader;pipeline.stage.pName="main";
    if(vkCreateComputePipelines(p->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&p->pipeline)!=VK_SUCCESS)return nullptr;
    return owned.release();
}
}
XrRootAliasProbe* xr_root_alias_probe_create(const XrRootAliasProbeInput& in){try{return root_probe_create(in);}catch(...){return nullptr;}}
bool xr_root_alias_probe_record(XrRootAliasProbe* p){
    if(!p||p->recorded||p->retained)return false;
    if(p->device!=dev.device){p->retained=true;return false;}
    if(vk_timeline_completed()>=p->input.recording)return false;
    XrUniformCopySource source;XrGuestRootSource root;const auto& in=p->input;
    if(!memory_xr_uniform_copy_source(in.camera.buffer,in.recording,source)||!memory_xr_guest_root_source(in.recording,root)||
       !root_probe_stamp(source.stamp,p->source.stamp)||!root_probe_stamp(root.stamp,p->root.stamp)||root.address!=p->root.address||root.bytes!=p->root.bytes||
       !xr_uniform_copy_source_valid(source,in.camera.buffer,p->page_begin,p->page_bytes,p->source.stamp))return false;
    p->recorded=true;end_pass(in.command);
    root_probe_order(in.command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for(unsigned eye=0;eye<2;++eye){VkBufferCopy copy{p->page_begin,0,p->page_bytes};vkCmdCopyBuffer(in.command,in.camera.buffer,p->pages[eye].buffer,1,&copy);}
    root_probe_order(in.command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for(unsigned eye=0;eye<2;++eye){VkBufferCopy copy{0,p->inner,in.camera.range};vkCmdCopyBuffer(in.command,in.matrices[eye],p->pages[eye].buffer,1,&copy);}
    vkCmdFillBuffer(in.command,p->readback.buffer,2*p->stride,32,0);
    root_probe_order(in.command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);
    vkCmdBindPipeline(in.command,VK_PIPELINE_BIND_POINT_COMPUTE,p->pipeline);
    for(unsigned eye=0;eye<2;++eye){hookshot::vr::EyeRootAliasParameters q{p->root.address,p->roots[eye].address,p->source_page_address,p->pages[eye].address,p->readback.address+2*p->stride+eye*16,uint32_t(root_probe_entries),uint32_t(p->page_bytes),0,0};
        vkCmdPushConstants(in.command,p->layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof q,&q);vkCmdDispatch(in.command,uint32_t((root_probe_entries+255)/256),1,1);
        root_probe_order(in.command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);q.mode=1;
        vkCmdPushConstants(in.command,p->layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof q,&q);vkCmdDispatch(in.command,uint32_t((root_probe_entries+255)/256),1,1);
        root_probe_order(in.command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);
        q={p->roots[eye].address,p->readback.address+eye*p->stride,in.guest_camera,p->pages[eye].address,p->readback.address+2*p->stride+eye*16,uint32_t(root_probe_entries),uint32_t(p->page_bytes),2,uint32_t(in.camera.range)};
        vkCmdPushConstants(in.command,p->layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof q,&q);vkCmdDispatch(in.command,uint32_t((in.camera.range/4+255)/256),1,1);
    }
    root_probe_order(in.command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);return true;
}
bool xr_root_alias_probe_retire(XrRootAliasProbe*& p,uint64_t completed,const std::array<const unsigned char*,2>& expected,size_t bytes,XrRootAliasProbeResult& out){
    if(!p)return true;if(p->retained)return false;
    if(p->device!=dev.device){p->retained=true;return false;}
    if(p->recorded&&completed<p->input.recording)return false;
    out={};out.recorded=p->recorded;out.allocated_bytes=p->allocated;
    if(p->recorded){invalidate_mapped(p->readback.memory,p->readback_bytes);const auto* data=static_cast<const unsigned char*>(p->mapped);
        bool verified=bytes==p->input.camera.range&&expected[0]&&expected[1];
        for(unsigned eye=0;eye<2;++eye){uint32_t counters[4]{};std::memcpy(counters,data+2*p->stride+eye*16,sizeof counters);
            out.aliases[eye]=counters[1];out.uncovered[eye]=counters[2];out.errors[eye]=uint64_t(counters[0])+counters[3];
            if(counters[0]||counters[2]||counters[3]||counters[1]<p->page_bytes/4096||!expected[eye]||bytes!=p->input.camera.range)verified=false;
            else if(std::memcmp(data+eye*p->stride,expected[eye],bytes)){verified=false;++out.errors[eye];}
        }out.verified=verified;
    }
    root_probe_dispose(p);p=nullptr;return true;
}
}
