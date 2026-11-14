// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_internal.h"
#include "vk_draw.h"
#include "xr_baseline_recorder.h"
#include "xr_uniform_copy_source.h"
#include "xr_eye_matrix_patch_spv.h"
#include "xr_matrix_patch_check.h"
#include "xr_shifted_draw_check.h"
#include <array>
#include <new>
#include <memory>
#include "vk_work.h"
namespace mkwvk {
struct XrBaselineRecorder {
    VkDevice device=VK_NULL_HANDLE;XrBaselineRecorderInput source;
    XrDescriptorSnapshot original;std::array<XrDescriptorSnapshot,2> descriptors,shift_descriptors;
    EyeImageClonePair color,depth,shift_color,shift_depth;EyeImageViewPair color_views,depth_views,shift_color_views,shift_depth_views;
    std::array<VkBuffer,2> uniforms{};std::array<VmaAllocation,2> uniform_memory{};
    std::array<VkBuffer,2> matrix_buffers{};std::array<VmaAllocation,2> matrix_memory{};
    VkShaderModule matrix_shader=VK_NULL_HANDLE;VkPipelineLayout matrix_layout=VK_NULL_HANDLE;VkPipeline matrix_pipeline=VK_NULL_HANDLE;
    VkDeviceAddress matrix_source=0;std::array<VkDeviceAddress,2> matrix_destinations{};
    uint64_t readback_bytes=0;
    VkBuffer readback=VK_NULL_HANDLE;VmaAllocation readback_memory=nullptr;void* mapped=nullptr;
    uint64_t frame_bytes=0,allocated=0;bool prepared=false,finished=false;
    XrRootAliasProbe* root_alias=nullptr;
    XrBaselineAttempt attempt;XrUniformSourceStamp source_stamp{};
};
namespace {
constexpr uint64_t budget=64ull*1024*1024;
uint64_t identity(VkDevice v){return uint64_t(reinterpret_cast<uintptr_t>(v));}
uint64_t identity(VkCommandBuffer v){return uint64_t(reinterpret_cast<uintptr_t>(v));}
bool allocate_image(const VkImageCreateInfo& ci,VkImage& image,uintptr_t& allocation){
    VmaAllocation memory=nullptr;if(!create_xr_private_image(ci,image,memory))return false;
    allocation=reinterpret_cast<uintptr_t>(memory);return true;
}
void release_image(VkImage image,uintptr_t allocation){destroy_image(image,reinterpret_cast<VmaAllocation>(allocation));}
uint64_t image_bytes(uintptr_t allocation){return allocation_size(reinterpret_cast<VmaAllocation>(allocation));}
void release_buffers(XrBaselineRecorder& p){
    for(unsigned eye=0;eye<2;++eye)if(p.uniforms[eye])destroy_buffer(p.uniforms[eye],p.uniform_memory[eye]);
    for(unsigned eye=0;eye<2;++eye)if(p.matrix_buffers[eye])destroy_buffer(p.matrix_buffers[eye],p.matrix_memory[eye]);
    if(p.matrix_pipeline)vkDestroyPipeline(p.device,p.matrix_pipeline,nullptr);
    if(p.matrix_layout)vkDestroyPipelineLayout(p.device,p.matrix_layout,nullptr);
    if(p.matrix_shader)vkDestroyShaderModule(p.device,p.matrix_shader,nullptr);
    if(p.readback)destroy_buffer(p.readback,p.readback_memory);
}
void dispose_unsubmitted(XrBaselineRecorder* p){
    XrRootAliasProbeResult root_result;xr_root_alias_probe_retire(p->root_alias,0,{},0,root_result);
    p->color.retire(0);p->depth.retire(0);p->shift_color.retire(0);p->shift_depth.retire(0);release_buffers(*p);delete p;
}
bool account(XrBaselineRecorder& p,VmaAllocation allocation){const auto n=allocation_size(allocation);if(!n||n>budget-p.allocated)return false;p.allocated+=n;return true;}
bool shape(const EyeImageSource& s,VkFormat format,VkImageAspectFlags aspect){
    return s.image&&s.identity&&s.format==format&&s.aspect==aspect&&!s.sparse&&s.type==VK_IMAGE_TYPE_2D&&s.samples==VK_SAMPLE_COUNT_1_BIT&&
        s.width&&s.height&&s.width<=4096&&s.height<=4096&&s.levels==1&&s.layers==1;
}
void order(VkCommandBuffer command,VkPipelineStageFlags2 dst,VkAccessFlags2 access){
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};barrier.srcStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;barrier.dstStageMask=dst;barrier.dstAccessMask=access;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&barrier;vkCmdPipelineBarrier2(command,&dependency);
}
void color_readback(XrBaselineRecorder& p,VkImage image,unsigned slot){
    order(p.source.command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkBufferImageCopy copy{};copy.bufferOffset=slot*p.frame_bytes;copy.imageSubresource={p.source.depth_reference?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageExtent={p.source.color.width,p.source.color.height,1};
    vkCmdCopyImageToBuffer(p.source.command,image,VK_IMAGE_LAYOUT_GENERAL,p.readback,1,&copy);
}
}
// Close overlapping resolved UBO ranges before allocating either eye. This is
// byte routing only, not proof that the full logical camera or shader semantics
// have been identified. A camera transform must still be separately reviewed.
static bool close_camera_aliases(XrBaselineRecorderInput& in){
    if(!in.camera.buffer||!in.camera.range||in.camera.range>65536||in.camera.offset>UINT64_MAX-in.camera.range)return false;
    auto begin=in.camera.offset,end=begin+in.camera.range;
    for(unsigned pass=0;pass<33;++pass){
        bool changed=false;
        for(const auto& w:in.writes){
            if(w.descriptorType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER&&w.descriptorType!=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)continue;
            if(!w.pBufferInfo||!w.descriptorCount||w.descriptorCount>32)return false;
            for(uint32_t element=0;element<w.descriptorCount;++element){const auto& b=w.pBufferInfo[element];
                if(b.buffer!=in.camera.buffer)continue;
                if(!b.range||b.range==VK_WHOLE_SIZE||b.offset>UINT64_MAX-b.range)return false;
                const auto limit=b.offset+b.range;if(b.offset>=end||limit<=begin)continue;
                // A storage alias could read or write the original camera during
                // a private draw; never silently leave such an alias shared.
                if(w.descriptorType!=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)return false;
                if(b.offset%4||b.range%4)return false;
                const auto next_begin=std::min(begin,b.offset),next_end=std::max(end,limit);
                if(next_end-next_begin>65536)return false;
                changed|=next_begin!=begin||next_end!=end;begin=next_begin;end=next_end;
                bool found=false;for(const auto& slot:in.camera_slots)found|=slot.binding==w.dstBinding&&slot.element==element;
                if(!found){if(in.camera_slots.size()>=32)return false;in.camera_slots.push_back({w.dstBinding,element});}
            }
        }
        if(!changed){in.camera.offset=begin;in.camera.range=end-begin;return true;}
    }
    return false;
}
static XrBaselineRecorder* create_recorder(const XrBaselineRecorderInput& requested){
    auto in=requested;
    // Validate and own the writer payload before examining raw descriptor arrays.
    XrDescriptorSnapshot checked;if(!checked.capture(in.writes))return nullptr;
    in.writes=checked.writes();
    if(in.view_shift_probe&&(!in.shifted_draw_probe||in.depth_reference==in.view_color_probe||!std::isfinite(in.view_shift_units)||in.view_shift_units<=0||in.view_shift_units>8))return nullptr;
    if(in.root_alias_probe&&(!in.matrix_probe||!in.view_shift_probe||in.view_color_probe||!in.guest_camera))return nullptr;
    if(in.view_color_probe&&!in.view_shift_probe)return nullptr;
    if(in.depth_reference&&!in.shifted_draw_probe)return nullptr;
    if(in.shifted_draw_probe&&(!in.matrix_probe||in.color.width<160))return nullptr;
    // A shader may consume only 0xb0 bytes of a much larger declared view block.
    // Extent comes from the same canonical NVN binding; mirror metadata must also
    // prove the entire resolved range before any allocation or command recording.
    if(in.matrix_probe){
        if(in.camera_object_bytes<0x230||in.camera_object_bytes>65536||in.camera_object_bytes%4||in.camera.range>in.camera_object_bytes)return nullptr;
        in.camera.range=in.camera_object_bytes;
    }
    const auto seed_offset=in.camera.offset;
    if(in.camera_slots.empty()||!close_camera_aliases(in))return nullptr;
    if(in.matrix_probe&&(in.camera.offset!=seed_offset||in.camera.offset%16||in.camera.range<0x230))return nullptr;
    if(par_active||!in.command||!in.layout||!in.serial||!in.recording||!in.program||!vkCmdPushDescriptorSetKHR||
       !in.geometry.count||in.geometry.count>1048576||!in.geometry.instances||in.geometry.instances>1024||
       !shape(in.color,VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT)||!shape(in.depth,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT)||
       in.color.image==in.depth.image||in.color.width!=in.depth.width||in.color.height!=in.depth.height||
       in.camera_slots.empty()||in.camera_slots.size()>32||!in.camera.buffer||!in.camera.range||in.camera.range>65536||in.camera.range%4||
       in.camera.offset%4||vk_timeline_completed()>=in.recording)return nullptr;
    XrUniformCopySource source;
    if(!memory_xr_uniform_copy_source(in.camera.buffer,in.recording,source)||!xr_uniform_copy_source_valid(source,in.camera.buffer,in.camera.offset,in.camera.range,source.stamp))return nullptr;
    if(in.matrix_probe&&!(source.usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))return nullptr;
    std::unique_ptr<XrBaselineRecorder,decltype(&dispose_unsubmitted)> owned(new(std::nothrow)XrBaselineRecorder,dispose_unsubmitted);
    auto* p=owned.get();if(!p)return nullptr;p->source_stamp=source.stamp;
    p->device=dev.device;p->source=in;p->frame_bytes=uint64_t(in.color.width)*in.color.height*4;
    if(p->frame_bytes>budget/4||!p->original.capture(in.writes)){return nullptr;}
    // Do not retain pointers into the mutable writer in the stored source input.
    p->source.writes.clear();
    for(unsigned index=0;index<in.camera_slots.size();++index){const auto slot=in.camera_slots[index];bool found=false;
        for(unsigned previous=0;previous<index;++previous)if(in.camera_slots[previous].binding==slot.binding&&in.camera_slots[previous].element==slot.element){return nullptr;}
        for(const auto& write:p->original.writes())if(write.dstBinding==slot.binding&&write.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER&&slot.element<write.descriptorCount){
            const auto& bound=write.pBufferInfo[slot.element];found=bound.buffer==in.camera.buffer&&bound.offset>=in.camera.offset&&bound.offset-in.camera.offset<in.camera.range&&bound.range&&bound.range!=VK_WHOLE_SIZE&&bound.range<=in.camera.range-(bound.offset-in.camera.offset);
        }
        if(!found){return nullptr;}
    }
    const EyeImageFactoryApi api{dev.device,vkCreateImage,vkDestroyImage,vkGetImageMemoryRequirements,allocate_image,release_image,image_bytes,vkCmdPipelineBarrier2,vkCmdCopyImage,vkCreateImageView,vkDestroyImageView};
    if(!p->color.create(in.color,api,budget)||!p->depth.create(in.depth,api,budget-p->color.allocated_bytes())){return nullptr;}
    p->allocated=p->color.allocated_bytes()+p->depth.allocated_bytes();
    auto views=[&](EyeImageClonePair& pair,const EyeImageSource& original,EyeImageViewPair& output){
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};info.image=original.image;info.viewType=VK_IMAGE_VIEW_TYPE_2D;info.format=original.format;
        info.subresourceRange={original.aspect,0,1,0,1};return pair.create_view_pair(info,output);
    };
    if(!views(p->color,in.color,p->color_views)||!views(p->depth,in.depth,p->depth_views)){return nullptr;}
    if(in.shifted_draw_probe){
        if(!p->shift_color.create(in.color,api,budget-p->allocated))return nullptr;
        p->allocated+=p->shift_color.allocated_bytes();
        if(!p->shift_depth.create(in.depth,api,budget-p->allocated))return nullptr;
        p->allocated+=p->shift_depth.allocated_bytes();
        if(!views(p->shift_color,in.color,p->shift_color_views)||!views(p->shift_depth,in.depth,p->shift_depth_views))return nullptr;
    }
    for(unsigned eye=0;eye<2;++eye){
        if(!create_device_buffer(in.camera.range,VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,p->uniforms[eye],p->uniform_memory[eye],true)||!account(*p,p->uniform_memory[eye])||!p->descriptors[eye].capture(p->original.writes())){return nullptr;}
        for(const auto slot:in.camera_slots){
            VkDeviceSize range=0,offset=0;for(const auto& write:p->original.writes())if(write.dstBinding==slot.binding){range=write.pBufferInfo[slot.element].range;offset=write.pBufferInfo[slot.element].offset-in.camera.offset;}
            if(!p->descriptors[eye].set_uniform(slot.binding,slot.element,{p->uniforms[eye],offset,range})){return nullptr;}
        }
    }
    if(in.matrix_probe){
        VkBufferDeviceAddressInfo address{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};address.buffer=in.camera.buffer;
        const auto base=vkGetBufferDeviceAddress(p->device,&address);
        if(!base||base>UINT64_MAX-in.camera.offset||base+in.camera.offset>UINT64_MAX-in.camera.range)return nullptr;
        p->matrix_source=base+in.camera.offset;if(p->matrix_source%16)return nullptr;
        for(unsigned eye=0;eye<2;++eye){
            if(!create_device_buffer(in.camera.range,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,p->matrix_buffers[eye],p->matrix_memory[eye],true)||!account(*p,p->matrix_memory[eye]))return nullptr;
            address.buffer=p->matrix_buffers[eye];const auto value=vkGetBufferDeviceAddress(p->device,&address);
            if(!value||value%16||value>UINT64_MAX-in.camera.range||(value<p->matrix_source+in.camera.range&&p->matrix_source<value+in.camera.range))return nullptr;
            if(eye&&value<p->matrix_destinations[0]+in.camera.range&&p->matrix_destinations[0]<value+in.camera.range)return nullptr;
            p->matrix_destinations[eye]=value;
            if(in.shifted_draw_probe){
                if(!p->shift_descriptors[eye].capture(p->original.writes()))return nullptr;
                for(const auto slot:in.camera_slots){
                    VkDeviceSize range=0,offset=0;for(const auto& write:p->original.writes())if(write.dstBinding==slot.binding){range=write.pBufferInfo[slot.element].range;offset=write.pBufferInfo[slot.element].offset-in.camera.offset;}
                    if(!p->shift_descriptors[eye].set_uniform(slot.binding,slot.element,{p->matrix_buffers[eye],offset,range}))return nullptr;
                }
            }
        }
        VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kEyeMatrixPatchSpv;shader.pCode=kEyeMatrixPatchSpv;
        if(vkCreateShaderModule(p->device,&shader,nullptr,&p->matrix_shader)!=VK_SUCCESS)return nullptr;
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,32};VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;
        if(vkCreatePipelineLayout(p->device,&layout,nullptr,&p->matrix_layout)!=VK_SUCCESS)return nullptr;
        VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=p->matrix_layout;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=p->matrix_shader;pipeline.stage.pName="main";
        if(vkCreateComputePipelines(p->device,VK_NULL_HANDLE,1,&pipeline,nullptr,&p->matrix_pipeline)!=VK_SUCCESS)return nullptr;
    }
    if(in.root_alias_probe){
        XrRootAliasProbeInput input;input.command=in.command;input.recording=in.recording;input.guest_camera=in.guest_camera;input.camera=in.camera;input.matrices=p->matrix_buffers;input.count=in.geometry.count;input.instances=in.geometry.instances;input.first=in.geometry.first;input.first_instance=in.geometry.first_instance;input.vertex_offset=in.geometry.vertex_offset;input.indexed=in.geometry.indexed;
        p->root_alias=xr_root_alias_probe_create(input);if(!p->root_alias)return nullptr;
    }
    p->readback_bytes=(in.shifted_draw_probe?6:4)*p->frame_bytes+(in.matrix_probe?4*in.camera.range:0);
    if(p->readback_bytes>budget-p->allocated)return nullptr;
    if(!create_buffer(p->readback_bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true,p->readback,p->readback_memory,&p->mapped,true)||!p->mapped||!account(*p,p->readback_memory)){return nullptr;}
    return owned.release();
}
XrBaselineRecorder* xr_baseline_recorder_create(const XrBaselineRecorderInput& in){
    try{return create_recorder(in);}catch(...){return nullptr;} // Optional diagnostics must not fail the ordinary source draw.
}
bool xr_baseline_recorder_prepare(XrBaselineRecorder* p){
    if(!p||p->prepared||p->device!=dev.device||par_active)return false;
    XrUniformCopySource source;const auto& in=p->source;
    if(!memory_xr_uniform_copy_source(in.camera.buffer,in.recording,source)||!xr_uniform_copy_source_valid(source,in.camera.buffer,in.camera.offset,in.camera.range,p->source_stamp)||vk_timeline_completed()>=in.recording)return false;
    if(!p->attempt.prepare(in.serial,in.recording,in.program,identity(p->device),identity(in.command)))return false;
    end_pass(in.command);p->prepared=true; // Every allocation is now protected by the actual queue value.
    const bool color=p->color.record_initial_copy(in.command,in.recording),depth=p->depth.record_initial_copy(in.command,in.recording);
    note_work(); // The image factory uses raw function pointers rather than vk_work wrappers.
    if(!color||!depth){p->attempt.abandon();return false;}
    if(in.shifted_draw_probe&&(!p->shift_color.record_initial_copy(in.command,in.recording)||!p->shift_depth.record_initial_copy(in.command,in.recording))){p->attempt.abandon();return false;}
    order(in.command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
    for(auto buffer:p->uniforms){VkBufferCopy copy{in.camera.offset,0,in.camera.range};vkCmdCopyBuffer(in.command,in.camera.buffer,buffer,1,&copy);}
    color_readback(*p,in.depth_reference?in.depth.image:in.color.image,0);
    if(in.matrix_probe){
        VkBufferCopy copy{in.camera.offset,(in.shifted_draw_probe?6:4)*p->frame_bytes,in.camera.range};vkCmdCopyBuffer(in.command,in.camera.buffer,p->readback,1,&copy);
        order(in.command,VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,VK_ACCESS_2_SHADER_READ_BIT|VK_ACCESS_2_SHADER_WRITE_BIT);
        vkCmdBindPipeline(in.command,VK_PIPELINE_BIND_POINT_COMPUTE,p->matrix_pipeline);
        for(unsigned eye=0;eye<2;++eye){
            struct Parameters {VkDeviceAddress source,destination;uint32_t bytes;float shift;uint32_t mode,reserved;};static_assert(sizeof(Parameters)==32);
            const Parameters parameters{p->matrix_source,p->matrix_destinations[eye],uint32_t(in.camera.range),in.view_shift_probe?(eye?in.view_shift_units:-in.view_shift_units):(in.shifted_draw_probe?(eye?-8.f:8.f)/in.color.width:(eye?.01f:-.01f)),in.view_color_probe?3u:(in.view_shift_probe?2u:(in.shifted_draw_probe?1u:0u)),in.view_color_probe?(in.color.width|(in.color.height<<16)):0};
            vkCmdPushConstants(in.command,p->matrix_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof parameters,&parameters);vkCmdDispatch(in.command,1,1,1);
        }
        // No compute descriptors are touched. Ordinary compute dispatches rebind
        // their pipeline and constants; graphics state remains on its own bind point.
    }
    if(in.root_alias_probe&&!xr_root_alias_probe_record(p->root_alias)){p->attempt.abandon();return false;}
    order(in.command,VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT);return true;
}
bool xr_baseline_recorder_finish(XrBaselineRecorder* p,uint64_t serial,uint64_t recording,uint64_t program,VkCommandBuffer command,const std::vector<VkWriteDescriptorSet>& writes){
    if(!p||!p->prepared||p->finished)return false;
    std::array<XrRootIndirectCommand,2> indirect{};
    if(p->source.root_alias_probe)for(unsigned eye=0;eye<2;++eye)if(!xr_root_alias_probe_indirect_command(p->root_alias,command,recording,eye,indirect[eye])){p->attempt.abandon();return false;}
    if(p->device!=dev.device||!p->original.matches(writes)||!p->attempt.finish(serial,recording,program,identity(dev.device),identity(command))){p->attempt.abandon();return false;}
    end_pass(command);color_readback(*p,p->source.depth_reference?p->source.depth.image:p->source.color.image,1);
    for(unsigned pass=0;pass<(p->source.shifted_draw_probe?2u:1u);++pass)for(unsigned eye=0;eye<2;++eye){
        order(command,VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT|VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT,VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT|VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT);
        VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO},depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        color.imageView=pass?p->shift_color_views.eyes[eye]:p->color_views.eyes[eye];depth.imageView=pass?p->shift_depth_views.eyes[eye]:p->depth_views.eyes[eye];
        color.imageLayout=depth.imageLayout=VK_IMAGE_LAYOUT_GENERAL;color.loadOp=depth.loadOp=VK_ATTACHMENT_LOAD_OP_LOAD;color.storeOp=depth.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};rendering.renderArea={{0,0},{p->source.color.width,p->source.color.height}};
        rendering.layerCount=1;rendering.colorAttachmentCount=1;rendering.pColorAttachments=&color;rendering.pDepthAttachment=&depth;
        vkCmdBeginRendering(command,&rendering);
        const auto& descriptors=(pass?p->shift_descriptors[eye]:p->descriptors[eye]).writes();vkCmdPushDescriptorSetKHR(command,VK_PIPELINE_BIND_POINT_GRAPHICS,p->source.layout,0,uint32_t(descriptors.size()),descriptors.data());
        const auto& g=p->source.geometry;
        if(pass&&p->source.root_alias_probe){
            if(g.indexed)vkCmdDrawIndexedIndirect(command,indirect[eye].buffer,indirect[eye].offset,1,sizeof(VkDrawIndexedIndirectCommand));
            else vkCmdDrawIndirect(command,indirect[eye].buffer,indirect[eye].offset,1,sizeof(VkDrawIndirectCommand));
        }else if(g.indexed)vkCmdDrawIndexed(command,g.count,g.instances,g.first,g.vertex_offset,g.first_instance);
        else vkCmdDraw(command,g.count,g.instances,g.first,g.first_instance);
        vkCmdEndRendering(command);color_readback(*p,p->source.depth_reference?(pass?p->shift_depth.eye(eye):p->depth.eye(eye)):(pass?p->shift_color.eye(eye):p->color.eye(eye)),eye+2+2*pass);
    }
    const auto& original=p->original.writes();vkCmdPushDescriptorSetKHR(command,VK_PIPELINE_BIND_POINT_GRAPHICS,p->source.layout,0,uint32_t(original.size()),original.data());
    if(p->source.matrix_probe){
        order(command,VK_PIPELINE_STAGE_2_COPY_BIT,VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT);
        const auto n=p->source.camera.range,offset=(p->source.shifted_draw_probe?6:4)*p->frame_bytes;
        VkBufferCopy copy{p->source.camera.offset,offset+n,n};vkCmdCopyBuffer(command,p->source.camera.buffer,p->readback,1,&copy);
        for(unsigned eye=0;eye<2;++eye){copy={0,offset+(eye+2)*n,n};vkCmdCopyBuffer(command,p->matrix_buffers[eye],p->readback,1,&copy);}
    }
    order(command,VK_PIPELINE_STAGE_2_HOST_BIT,VK_ACCESS_2_HOST_READ_BIT);
    p->finished=true;return true;
}
void xr_baseline_recorder_abandon(XrBaselineRecorder* p){if(p)p->attempt.abandon();}
bool xr_baseline_recorder_retire(XrBaselineRecorder*& p,XrBaselineComparison& comparison){
    if(!p)return false;
    if(!p->prepared){dispose_unsubmitted(p);p=nullptr;comparison={};return true;}
    const bool finished=p->finished;const auto completed=vk_timeline_completed();
    if(!p->attempt.retire(identity(dev.device),completed,p->device==dev.device))return false;
    comparison={};if(finished){invalidate_mapped(p->readback_memory,p->readback_bytes);comparison=compare_xr_baseline(p->mapped,size_t(4*p->frame_bytes),p->source.color.width,p->source.color.height);
        if(p->source.matrix_probe){const auto n=size_t(p->source.camera.range);const auto* bytes=static_cast<const unsigned char*>(p->mapped)+(p->source.shifted_draw_probe?6:4)*p->frame_bytes;
            comparison.depth_reference=p->source.depth_reference;comparison.matrix_probe_requested=true;comparison.matrix_payload_bytes=n;comparison.matrix_source_unchanged=!std::memcmp(bytes,bytes+n,n);
            comparison.matrix_probe_validated=xr_matrix_patch_payloads(bytes,4*n,n,p->source.shifted_draw_probe&&!p->source.view_shift_probe, p->source.view_shift_probe?p->source.view_shift_units:(p->source.shifted_draw_probe?8.f/p->source.color.width:.01f),p->source.view_shift_probe,p->source.view_color_probe,p->source.color.width,p->source.color.height);
        }
    }
    if(finished&&p->source.shifted_draw_probe){const auto shifted=compare_xr_shifted_draw(p->mapped,size_t(6*p->frame_bytes),p->source.color.width,p->source.color.height,p->source.view_shift_probe);
        comparison.view_color_requested=p->source.view_color_probe;comparison.view_shift_requested=p->source.view_shift_probe;comparison.view_shift_units=p->source.view_shift_units;comparison.shifted_draw_requested=true;comparison.shifted_draw_verified=shifted.verified;comparison.shifted_centroid_x=shifted.x;comparison.shifted_centroid_y=shifted.y;comparison.shifted_changed_pixels=shifted.changed;
    }
    if(p->source.root_alias_probe){
        std::array<const unsigned char*,2> expected{};const auto n=size_t(p->source.camera.range);
        if(finished){const auto* bytes=static_cast<const unsigned char*>(p->mapped)+(p->source.shifted_draw_probe?6:4)*p->frame_bytes;expected={bytes+2*n,bytes+3*n};}
        XrRootAliasProbeResult root_result;if(!xr_root_alias_probe_retire(p->root_alias,completed,expected,finished?n:0,root_result))return false;
        comparison.root_indirect_draws_recorded=finished&&p->source.shifted_draw_probe;comparison.root_indirect_verified=root_result.indirect_guard_verified;comparison.root_indirect_counts=root_result.indirect_counts;comparison.root_alias_requested=true;comparison.root_alias_verified=root_result.verified;comparison.root_alias_allocated_bytes=root_result.allocated_bytes;
        comparison.root_alias_counts=root_result.aliases;comparison.root_alias_uncovered=root_result.uncovered;comparison.root_alias_errors=root_result.errors;
    }
    if(!p->color.retire(completed)||!p->depth.retire(completed)||!p->shift_color.retire(completed)||!p->shift_depth.retire(completed))return false;
    release_buffers(*p);delete p;p=nullptr;return true;
}
}
