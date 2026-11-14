// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <type_traits>
namespace {
template<class T>T ph(uintptr_t value){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(value);else return T(value);}
std::vector<uint8_t> preview_memory(preview_w()*preview_h()*4);
bool stereo_preview(){return mode("preview_immersive_stereo")||mode("preview_immersive_stereo_missing");}
bool preview_acquired=false,preview_waited=false,gpu_pending=false;
uint32_t preview_index=0;
XrResult XRAPI_CALL preview_formats(XrSession,uint32_t cap,uint32_t* count,int64_t* out) {
    *count=1;if(cap)out[0]=pair_preview()?(mode("preview_pair_no_format")?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM):mode("preview_no_format")?VK_FORMAT_B8G8R8A8_UNORM:VK_FORMAT_R8G8B8A8_SRGB;return XR_SUCCESS;
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
    preview_acquired=true;preview_waited=false;*out=(mode("preview_invalid_index")||mode("preview_pair_invalid_index"))?99:preview_index++%3;++counts[12];return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_wait(XrSwapchain,const XrSwapchainImageWaitInfo*) {
    if(queue_check&&queue_check())++counts[5];
    if(mode("preview_image_timeout")||mode("preview_pair_image_timeout"))return XR_TIMEOUT_EXPIRED;
    if(mode("preview_pair_image_deferred")&&++counts[24]==1)return XR_TIMEOUT_EXPIRED;
    preview_waited=true;return XR_SUCCESS;
}
XrResult XRAPI_CALL preview_release(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    if(!queue_check||!queue_check()||!preview_acquired||!preview_waited||gpu_pending)++counts[5];
    if(mode("preview_pair_release_error"))return XR_ERROR_RUNTIME_FAILURE;
    preview_acquired=false;++counts[13];return XR_SUCCESS;
}
void VKAPI_CALL pv_queue(VkDevice,uint32_t family,uint32_t index,VkQueue* out) {if(family!=7||index)++counts[5];*out=ph<VkQueue>(500);}
VkResult VKAPI_CALL pv_buffer(VkDevice,const VkBufferCreateInfo* info,const VkAllocationCallbacks*,VkBuffer* out) {
    if(info->size!=preview_w()*preview_h()*4*(stereo_preview()?2:1)||info->usage!=VK_BUFFER_USAGE_TRANSFER_SRC_BIT)++counts[5];preview_memory.resize(size_t(info->size));*out=ph<VkBuffer>(501);++counts[15];return VK_SUCCESS;
}
void VKAPI_CALL pv_requirements(VkDevice,VkBuffer,VkMemoryRequirements* out){*out={VkDeviceSize(preview_w()*preview_h()*4*(stereo_preview()?2:1)),256,1};}
void VKAPI_CALL pv_memory_properties(VkPhysicalDevice,VkPhysicalDeviceMemoryProperties* out){*out={};out->memoryTypeCount=1;
    out->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|(mode("preview_noncoherent")?0:VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);}
VkResult VKAPI_CALL pv_allocate(VkDevice,const VkMemoryAllocateInfo*,const VkAllocationCallbacks*,VkDeviceMemory* out){
    if(mode("preview_allocation_error"))return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=ph<VkDeviceMemory>(502);return VK_SUCCESS;}
VkResult VKAPI_CALL pv_bind(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_map(VkDevice,VkDeviceMemory,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){*out=preview_memory.data();return VK_SUCCESS;}
void VKAPI_CALL pv_unmap(VkDevice,VkDeviceMemory){if(gpu_pending)++counts[5];}
void VKAPI_CALL pv_free(VkDevice,VkDeviceMemory,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
void VKAPI_CALL pv_destroy_buffer(VkDevice,VkBuffer,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];++counts[16];}
VkResult VKAPI_CALL pv_pool(VkDevice,const VkCommandPoolCreateInfo* info,const VkAllocationCallbacks*,VkCommandPool* out){if(info->queueFamilyIndex!=7)++counts[5];if(mode("preview_pair_pool_error"))return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=ph<VkCommandPool>(503);return VK_SUCCESS;}
void VKAPI_CALL pv_destroy_pool(VkDevice,VkCommandPool,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL pv_commands(VkDevice,const VkCommandBufferAllocateInfo*,VkCommandBuffer* out){*out=ph<VkCommandBuffer>(504);return VK_SUCCESS;}
VkResult VKAPI_CALL pv_reset_command(VkCommandBuffer,VkCommandBufferResetFlags){if(gpu_pending)++counts[5];return VK_SUCCESS;}
VkResult VKAPI_CALL pv_begin(VkCommandBuffer,const VkCommandBufferBeginInfo*){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_end(VkCommandBuffer){return VK_SUCCESS;}
VkResult VKAPI_CALL pv_fence(VkDevice,const VkFenceCreateInfo*,const VkAllocationCallbacks*,VkFence* out){*out=ph<VkFence>(505);return VK_SUCCESS;}
void VKAPI_CALL pv_destroy_fence(VkDevice,VkFence,const VkAllocationCallbacks*){if(gpu_pending)++counts[5];}
VkResult VKAPI_CALL pv_reset_fence(VkDevice,uint32_t,const VkFence*){if(gpu_pending)++counts[5];return VK_SUCCESS;}
VkResult VKAPI_CALL pv_wait(VkDevice,uint32_t,const VkFence*,VkBool32,uint64_t){
    if(queue_check&&queue_check())++counts[5];
    if(mode("preview_gpu_timeout")||mode("preview_pair_gpu_timeout"))return VK_TIMEOUT;
    if((mode("preview_pair_gpu_deferred")||mode("preview_pair_origin_pending"))&&++counts[23]<=3)return VK_TIMEOUT;gpu_pending=false;return VK_SUCCESS;}
VkResult VKAPI_CALL pv_submit(VkQueue,uint32_t,const VkSubmitInfo*,VkFence){if(!queue_check||!queue_check())++counts[5];
    if(mode("preview_submit_error")||mode("preview_pair_submit_error"))return VK_ERROR_DEVICE_LOST;gpu_pending=true;++counts[14];return VK_SUCCESS;}
void VKAPI_CALL pv_barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*){}
void VKAPI_CALL pv_copy(VkCommandBuffer,VkBuffer buffer,VkImage,VkImageLayout layout,uint32_t count,const VkBufferImageCopy* copy){
    if(pair_preview()){
        if(!preview_waited||buffer!=ph<VkBuffer>(900)||layout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||count!=2||copy[0].bufferOffset||copy[0].imageOffset.x||copy[0].imageExtent.width!=8||copy[0].imageExtent.height!=8||copy[1].bufferOffset!=256||copy[1].imageOffset.x!=8||copy[1].imageExtent.width!=8||copy[1].imageExtent.height!=8)++counts[5];++counts[22];return;
    }
    if(!preview_waited||layout!=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL||count!=(stereo_preview()?2u:1u)||copy->imageExtent.width!=preview_w()||copy->imageExtent.height!=preview_h()||preview_memory[0]!=17||preview_memory[1]!=34||preview_memory[2]!=51||preview_memory[3]!=255)++counts[5];
    if(stereo_preview()){
        const size_t offset=preview_w()*preview_h()*4;
        if(count!=2||copy[0].bufferOffset||copy[0].imageOffset.x||copy[1].bufferOffset!=offset||copy[1].imageOffset.x!=preview_w()||copy[1].imageExtent.width!=preview_w()||copy[1].imageExtent.height!=preview_h()||preview_memory[offset]!=85||preview_memory[offset+1]!=102||preview_memory[offset+2]!=119||preview_memory[offset+3]!=255)++counts[5];else ++counts[21];
    }
}
VkResult VKAPI_CALL pv_flush(VkDevice,uint32_t count,const VkMappedMemoryRange* range){if(count!=1||range->size!=VK_WHOLE_SIZE)++counts[5];++counts[18];return VK_SUCCESS;}
PFN_vkVoidFunction VKAPI_CALL preview_device_proc(VkDevice,const char* name) {
#define MAPV(n,f) if(!std::strcmp(name,"vk" #n))return reinterpret_cast<PFN_vkVoidFunction>(f);
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