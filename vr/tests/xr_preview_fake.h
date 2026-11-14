// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <type_traits>
#include "../../gpu/vk/xr_depth_pair_parameters.h"
namespace {
template<class T>T ph(uintptr_t value){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(value);else return T(value);}
std::vector<uint8_t> preview_memory(preview_w()*preview_h()*4);
bool stereo_preview(){return mode("preview_immersive_stereo")||mode("preview_immersive_stereo_missing");}
bool preview_acquired=false,preview_waited=false,gpu_pending=false;
uint32_t preview_index=0;
XrResult XRAPI_CALL preview_formats(XrSession,uint32_t cap,uint32_t* count,int64_t* out) {
    *count=1;if(cap)out[0]=pair_preview()?(pair_case("no_format")?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM):mode("preview_no_format")?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_R8G8B8A8_SRGB;return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_create(XrSession,const XrSwapchainCreateInfo* info,XrSwapchain* out) {
    if(info->width!=((stereo_preview()||pair_preview())?2*preview_w():preview_w())||info->height!=preview_h()||info->arraySize!=1||info->format!=(pair_preview()?VK_FORMAT_R8G8B8A8_UNORM:VK_FORMAT_R8G8B8A8_SRGB)||!(info->usageFlags&XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT))++counts[5];
    *out=ph<XrSwapchain>(300);++counts[10];return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_destroy(XrSwapchain) {if(gpu_pending)++counts[5];++counts[11];preview_acquired=false;return XR_SUCCESS;}
XrResult XRAPI_CALL preview_images(XrSwapchain,uint32_t cap,uint32_t* count,XrSwapchainImageBaseHeader* out) {
    *count=3;if(cap){auto* images=reinterpret_cast<XrSwapchainImageVulkan2KHR*>(out);for(uint32_t i=0;i<3;++i)images[i].image=ph<VkImage>(400+i);}return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_acquire(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* out) {
    if(!queue_check||!queue_check()||preview_acquired)++counts[5];
    preview_acquired=true;preview_waited=false;*out=(mode("preview_invalid_index")||pair_case("invalid_index"))?99:preview_index++%3;++counts[12];return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_wait(XrSwapchain,const XrSwapchainImageWaitInfo*) {
    if(queue_check&&queue_check())++counts[5];
    if(mode("preview_image_timeout")||pair_case("image_timeout"))return XR_TIMEOUT_EXPIRED;
    if(pair_case("image_deferred")&&++counts[24]==1)return XR_TIMEOUT_EXPIRED;
    preview_waited=true;return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_release(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    if(!queue_check||!queue_check()||!preview_acquired||!preview_waited||gpu_pending)++counts[5];
    if(pair_case("release_error"))return XR_ERROR_RUNTIME_FAILURE;
    if(late_preview())for(unsigned i=0;i<2;++i)late_released_x[i]=late_recorded_x[i];preview_acquired=false;++counts[13];return XR_SUCCESS;
}
void VKAPI_CALL pv_queue(VkDevice,uint32_t family,uint32_t index,VkQueue* out) {if(family!=7||index)++counts[5];*out=ph<VkQueue>(500);}
VkResult VKAPI_CALL pv_buffer(VkDevice,const VkBufferCreateInfo* info,const VkAllocationCallbacks*,VkBuffer* out) {
    if(late_preview()){if(info->size!=768||!(info->usage&VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))++counts[5];*out=ph<VkBuffer>(501);++counts[15];return VK_SUCCESS;}
    if(info->size!=preview_w()*preview_h()*4*(stereo_preview()?2:1)||info->usage!=VK_BUFFER_USAGE_TRANSFER_SRC_BIT)++counts[5];preview_memory.resize(size_t(info->size));*out=ph<VkBuffer>(501);++counts[15];return VK_SUCCESS;
}
void VKAPI_CALL pv_requirements(VkDevice,VkBuffer,VkMemoryRequirements* out){*out={late_preview()?768:VkDeviceSize(preview_w()*preview_h()*4*(stereo_preview()?2:1)),256,1};}
void VKAPI_CALL pv_memory_properties(VkPhysicalDevice,VkPhysicalDeviceMemoryProperties* out){*out={};out->memoryTypeCount=1;
    out->memoryTypes[0].propertyFlags=(late_preview()?VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT:VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)|(mode("preview_noncoherent")?0:VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);}
VkResult VKAPI_CALL pv_allocate(VkDevice,const VkMemoryAllocateInfo*,const VkAllocationCallbacks*,VkDeviceMemory* out){
    if(mode("preview_allocation_error")||pair_case("allocation_error"))return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=ph<VkDeviceMemory>(502);return VK_SUCCESS;}
VkResult VKAPI_CALL pv_bind(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_map(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){*out=preview_memory.data();return VK_SUCCESS;}
void VKAPI_CALL pv_unmap(VkDevice,VkDeviceMemory){if(gpu_pending)++counts[5];}
void VKAPI_CALL pv_free(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
void VKAPI_CALL pv_destroy_buffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];++counts[16];}
VkResult VKAPI_CALL pv_pool(VkDevice,const VkCommandPoolCreateInfo* info,const VkAllocationCallbacks*,VkCommandPool* out){if(info->queueFamilyIndex!=7)++counts[5];if(pair_case("pool_error"))return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=ph<VkCommandPool>(503);return VK_SUCCESS;}
void VKAPI_CALL pv_destroy_pool(VkDevice,VkCommandPool,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL pv_commands(VkDevice,const VkCommandBufferAllocateInfo*,VkCommandBuffer* out){*out=ph<VkCommandBuffer>(504);return VK_SUCCESS;}
VkResult VKAPI_CALL pv_reset_command(VkCommandBuffer,VkCommandBufferResetFlags){if(gpu_pending)++counts[5];return VK_SUCCESS;}
VkResult VKAPI_CALL pv_begin(VkCommandBuffer,const VkCommandBufferBeginInfo*){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_end(VkCommandBuffer){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_fence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=ph<VkFence>(505);return VK_SUCCESS;}
void VKAPI_CALL pv_destroy_fence(VkDevice,VkFence,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL pv_reset_fence(VkDevice,uint32_t,const VkFence*){if(gpu_pending)++counts[5];return VK_SUCCESS;}
VkResult VKAPI_CALL pv_wait(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t timeout){
    if(queue_check&&queue_check())++counts[5];
    if(pair_case("every_frame_deferred")&&timeout>0&&timeout<250000000)return VK_TIMEOUT;
    if(mode("preview_gpu_timeout")||pair_case("gpu_timeout"))return VK_TIMEOUT;
    if((pair_case("gpu_deferred")||pair_case("origin_pending"))&&++counts[23]<=3)return VK_TIMEOUT;gpu_pending=false;return VK_SUCCESS;}
VkResult VKAPI_CALL pv_submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence){if(!queue_check||!queue_check())++counts[5];
    if(mode("preview_submit_error")||pair_case("submit_error"))return VK_ERROR_DEVICE_LOST;gpu_pending=true;++counts[14];return VK_SUCCESS;}
void VKAPI_CALL pv_barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){}
void VKAPI_CALL pv_copy(VkCommandBuffer,VkBuffer buffer,VkImage,VkImageLayout layout,uint32_t count,const VkBufferImageCopy* copy){
    if(pair_preview()){
        if(!preview_waited||buffer!=ph<VkBuffer>(late_preview()?501:900)||layout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||count!=2||copy[0].bufferOffset||copy[0].imageOffset.x||copy[0].imageExtent.width!=8||copy[0].imageExtent.height!=8||copy[1].bufferOffset!=256||copy[1].imageOffset.x!=8||copy[1].imageExtent.width!=8||copy[1].imageExtent.height!=8)++counts[5];++counts[22];return;
    }
    if(!preview_waited||layout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||count!=(stereo_preview()?2u:1u)||copy->imageExtent.width!=preview_w()||copy->imageExtent.height!=preview_h()||preview_memory[0]!=17||preview_memory[1]!=34||preview_memory[2]!=51||preview_memory[3]!=255)++counts[5];
    if(stereo_preview()){
        const size_t offset=preview_w()*preview_h()*4;
        if(count!=2||copy[0].bufferOffset||copy[0].imageOffset.x||copy[1].bufferOffset!=offset||copy[1].imageOffset.x!=preview_w()||copy[1].imageExtent.width!=preview_w()||copy[1].imageExtent.height!=preview_h()||preview_memory[offset]!=85||preview_memory[offset+1]!=102||preview_memory[offset+2]!=119||preview_memory[offset+3]!=255)++counts[5];else ++counts[21];
    }
}
VkResult VKAPI_CALL pv_flush(VkDevice,uint32_t count,const VkMappedMemoryRange* range){if(count!=1||range->size!=VK_WHOLE_SIZE)++counts[5];++counts[18];return VK_SUCCESS;}
VkDeviceAddress VKAPI_CALL lp_address(VkDevice,const VkBufferDeviceAddressInfo*){return 0x40000;}
VkResult VKAPI_CALL lp_shader(VkDevice,const VkShaderModuleCreateInfo*,const VkAllocationCallbacks*,VkShaderModule* out){*out=ph<VkShaderModule>(600);return VK_SUCCESS;}
void VKAPI_CALL lp_destroy_shader(VkDevice,VkShaderModule,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL lp_layout(VkDevice,const VkPipelineLayoutCreateInfo*,const VkAllocationCallbacks*,VkPipelineLayout* out){*out=ph<VkPipelineLayout>(601);return VK_SUCCESS;}
void VKAPI_CALL lp_destroy_layout(VkDevice,VkPipelineLayout,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL lp_pipeline(VkDevice,VkPipelineCache,uint32_t,const VkComputePipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline* out){if(pair_case("pipeline_error"))return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=ph<VkPipeline>(602);return VK_SUCCESS;}
void VKAPI_CALL lp_destroy_pipeline(VkDevice,VkPipeline,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
void VKAPI_CALL lp_update(VkCommandBuffer,VkBuffer buffer,VkDeviceSize offset,VkDeviceSize size,const void* data){
 if(buffer!=ph<VkBuffer>(501)||offset!=512||size!=256)++counts[5];const auto* p=static_cast<const hookshot::vr::DepthPairParameters*>(data);for(unsigned eye=0;eye<2;++eye){if(p[eye].settings[2]!=1||p[eye].settings[3]!=8||p[eye].dimensions!=std::array<uint32_t,4>{8,8,8,8})++counts[5];late_recorded_x[eye]=p[eye].source_from_eye[3];}++counts[25];}
void VKAPI_CALL lp_bind(VkCommandBuffer,VkPipelineBindPoint,VkPipeline){}
void VKAPI_CALL lp_push(VkCommandBuffer,VkPipelineLayout,VkShaderStageFlags,uint32_t,uint32_t size,const void* data){
 const auto* d=static_cast<const hookshot::vr::DepthPairDispatch*>(data);if(size!=32||d->source!=0x20000||(d->destination!=0x40000&&d->destination!=0x40100)||d->parameters!=(d->destination==0x40000?0x40200:0x40280)||d->counters)++counts[5];}
void VKAPI_CALL lp_dispatch(VkCommandBuffer,uint32_t x,uint32_t y,uint32_t z){if(x!=1||y!=1||z!=1)++counts[5];++counts[26];}
PFN_vkVoidFunction VKAPI_CALL preview_device_proc(VkDevice,const char* name) {
#define MAPV(n,f) if(!std::strcmp(name,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f);
    MAPV(GetBufferDeviceAddress,lp_address) MAPV(CreateShaderModule,lp_shader) MAPV(DestroyShaderModule,lp_destroy_shader)
    MAPV(CreatePipelineLayout,lp_layout) MAPV(DestroyPipelineLayout,lp_destroy_layout) MAPV(CreateComputePipelines,lp_pipeline) MAPV(DestroyPipeline,lp_destroy_pipeline)
    MAPV(CmdUpdateBuffer,lp_update) MAPV(CmdBindPipeline,lp_bind) MAPV(CmdPushConstants,lp_push) MAPV(CmdDispatch,lp_dispatch)
    MAPV(GetDeviceQueue,pv_queue) MAPV(CreateBuffer,pv_buffer) MAPV(GetBufferMemoryRequirements,pv_requirements)
    MAPV(AllocateMemory,pv_allocate) MAPV(BindBufferMemory,pv_bind) MAPV(MapMemory,pv_map) MAPV(UnmapMemory,pv_unmap)
    MAPV(FreeMemory,pv_free) MAPV(DestroyBuffer,pv_destroy_buffer) MAPV(CreateCommandPool,pv_pool) MAPV(DestroyCommandPool,pv_destroy_pool)
    MAPV(AllocateCommandBuffers,pv_commands) MAPV(ResetCommandBuffer,pv_reset_command) MAPV(BeginCommandBuffer,pv_begin)
    MAPV(EndCommandBuffer,pv_end) MAPV(CreateFence,pv_fence) MAPV(DestroyFence,pv_destroy_fence) MAPV(ResetFences,pv_reset_fence)
    MAPV(WaitForFences,pv_wait) MAPV(QueueSubmit,pv_submit) MAPV(CmdPipelineBarrier,pv_barrier) MAPV(CmdCopyBufferToImage,pv_copy)
    MAPV(FlushMappedMemoryRanges,pv_flush)
#undef MAPV
    return nullptr;
}
PFN_vkVoidFunction VKAPI_CALL preview_instance_proc(VkInstance,const char* name) {
    if(!std::strcmp(name,"vkGetDeviceProcAddr"))return reinterpret_cast<PFN_vkVoidFunction>(preview_device_proc);
    if(!std::strcmp(name,"vkGetPhysicalDeviceMemoryProperties"))return reinterpret_cast<PFN_vkVoidFunction>(pv_memory_properties);
    return nullptr;
}
}