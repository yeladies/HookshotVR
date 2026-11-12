// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
namespace hookshot::vr {
struct EyeResourceApi {
    PFN_vkCreateBuffer create_buffer=nullptr;PFN_vkDestroyBuffer destroy_buffer=nullptr;
    PFN_vkGetBufferMemoryRequirements buffer_requirements=nullptr;PFN_vkBindBufferMemory bind_buffer=nullptr;
    PFN_vkCreateImage create_image=nullptr;PFN_vkDestroyImage destroy_image=nullptr;
    PFN_vkGetImageMemoryRequirements image_requirements=nullptr;PFN_vkBindImageMemory bind_image=nullptr;
    PFN_vkAllocateMemory allocate=nullptr;PFN_vkFreeMemory free=nullptr;
    PFN_vkCreateFence create_fence=nullptr;PFN_vkDestroyFence destroy_fence=nullptr;
    PFN_vkQueueSubmit submit=nullptr;PFN_vkWaitForFences wait=nullptr;PFN_vkResetFences reset=nullptr;
    PFN_vkGetBufferDeviceAddress buffer_address=nullptr;
    bool valid()const{return create_buffer&&destroy_buffer&&buffer_requirements&&bind_buffer&&create_image&&
        destroy_image&&image_requirements&&bind_image&&allocate&&free&&create_fence&&destroy_fence&&submit&&wait&&reset;}
};
struct OwnedEyeResource {
    VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory buffer_memory=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE;VkDeviceMemory image_memory=VK_NULL_HANDLE;
    VkDeviceAddress address=0;
};
// Single-in-flight owned resource experiment, independent of the game's replay path.
// Caller owns the device and queue synchronization and must keep them alive longer
// than this object. Destruction never frees a resource whose completion is unknown.
// A pending destruction intentionally retains allocations until device teardown.
class EyeResourcePair {
    EyeResourceApi api_{};VkDevice device_=VK_NULL_HANDLE;VkFence fence_=VK_NULL_HANDLE;
    std::array<OwnedEyeResource,2> eyes_{};uint64_t bytes_=0;
    bool ready_=false,pending_=false,submitted_=false;
    bool memory(const VkMemoryRequirements& requirements,const VkPhysicalDeviceMemoryProperties& properties,
                uint64_t budget,VkDeviceMemory& out,bool addressable=false) {
        if(!requirements.size||!requirements.alignment||requirements.size>budget-bytes_)return false;
        for(uint32_t i=0;i<properties.memoryTypeCount&&i<VK_MAX_MEMORY_TYPES;++i) {
            if(!(requirements.memoryTypeBits&(1u<<i))||!(properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))continue;
            const uint32_t heap=properties.memoryTypes[i].heapIndex;
            if(heap>=properties.memoryHeapCount||requirements.size>properties.memoryHeaps[heap].size)continue;
            VkMemoryAllocateInfo info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};
            flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            if(addressable)info.pNext=&flags;
            info.allocationSize=requirements.size;info.memoryTypeIndex=i;
            if(api_.allocate(device_,&info,nullptr,&out)!=VK_SUCCESS)return false;
            bytes_+=requirements.size;return true;
        }
        return false;
    }
    void release() {
        if(!device_||pending_)return;
        for(auto& eye:eyes_) {
            if(eye.image)api_.destroy_image(device_,eye.image,nullptr);
            if(eye.buffer)api_.destroy_buffer(device_,eye.buffer,nullptr);
            if(eye.image_memory)api_.free(device_,eye.image_memory,nullptr);
            if(eye.buffer_memory)api_.free(device_,eye.buffer_memory,nullptr);
            eye={};
        }
        if(fence_)api_.destroy_fence(device_,fence_,nullptr);
        fence_=VK_NULL_HANDLE;bytes_=0;ready_=false;
    }
public:
    EyeResourcePair()=default;
    EyeResourcePair(const EyeResourcePair&)=delete;EyeResourcePair& operator=(const EyeResourcePair&)=delete;
    ~EyeResourcePair(){if(pending_)poll(0);release();}
    // Addressable mode requires bufferDeviceAddress enabled on the supplied device.
    bool create(VkDevice device,const EyeResourceApi& api,const VkPhysicalDeviceMemoryProperties& properties,
                uint32_t width,uint32_t height,VkFormat format,uint64_t buffer_bytes,uint64_t budget,bool addressable=false) {
        if(device_||(addressable&&!api.buffer_address)||!device||!api.valid()||!width||!height||width>16384||height>16384||format==VK_FORMAT_UNDEFINED||
           !buffer_bytes||!budget||budget>512ull*1024*1024||properties.memoryTypeCount>VK_MAX_MEMORY_TYPES||
           properties.memoryHeapCount>VK_MAX_MEMORY_HEAPS)return false;
        api_=api;device_=device;
        for(auto& eye:eyes_) {
            VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=buffer_bytes;
            buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            if(addressable)buffer.usage|=VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            buffer.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
            if(api_.create_buffer(device_,&buffer,nullptr,&eye.buffer)!=VK_SUCCESS){release();return false;}
            VkMemoryRequirements requirements{};api_.buffer_requirements(device_,eye.buffer,&requirements);
            if(!memory(requirements,properties,budget,eye.buffer_memory,addressable)||api_.bind_buffer(device_,eye.buffer,eye.buffer_memory,0)!=VK_SUCCESS){release();return false;}
            if(addressable){
                VkBufferDeviceAddressInfo address_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};address_info.buffer=eye.buffer;
                eye.address=api_.buffer_address(device_,&address_info);
                if(!eye.address||eye.address%16||eye.address>UINT64_MAX-buffer_bytes){release();return false;}
                if(&eye==&eyes_[1]&&eye.address<eyes_[0].address+buffer_bytes&&eyes_[0].address<eye.address+buffer_bytes){release();return false;}
            }
            VkImageCreateInfo image{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};image.imageType=VK_IMAGE_TYPE_2D;
            image.format=format;image.extent={width,height,1};image.mipLevels=1;image.arrayLayers=1;
            image.samples=VK_SAMPLE_COUNT_1_BIT;image.tiling=VK_IMAGE_TILING_OPTIMAL;
            image.usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            image.sharingMode=VK_SHARING_MODE_EXCLUSIVE;image.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
            if(api_.create_image(device_,&image,nullptr,&eye.image)!=VK_SUCCESS){release();return false;}
            api_.image_requirements(device_,eye.image,&requirements);
            if(!memory(requirements,properties,budget,eye.image_memory)||api_.bind_image(device_,eye.image,eye.image_memory,0)!=VK_SUCCESS){release();return false;}
        }
        VkFenceCreateInfo info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if(api_.create_fence(device_,&info,nullptr,&fence_)!=VK_SUCCESS){release();return false;}
        ready_=true;return true;
    }
    // No implicit initialization/layout transition: caller records copies/barriers.
    // One submission per explicit rearm, on an externally synchronized queue of this device.
    VkResult submit(VkQueue queue,VkCommandBuffer command) {
        if(!ready_||submitted_||!queue||!command)return VK_ERROR_INITIALIZATION_FAILED;
        VkSubmitInfo info{VK_STRUCTURE_TYPE_SUBMIT_INFO};info.commandBufferCount=1;info.pCommandBuffers=&command;
        const auto result=api_.submit(queue,1,&info,fence_);
        // Device loss leaves execution uncertain; retain until device teardown.
        if(result==VK_SUCCESS||result==VK_ERROR_DEVICE_LOST){pending_=true;submitted_=true;}
        return result;
    }
    bool poll(uint64_t timeout_ns) {
        if(!pending_)return ready_;
        if(api_.wait(device_,1,&fence_,VK_TRUE,timeout_ns)!=VK_SUCCESS)return false;
        pending_=false;return true;
    }
    // Explicit reuse only after successful completion. The caller must reset/re-record
    // its command buffer and manage existing image layouts and new payloads itself.
    bool rearm() {
        if(!ready_||!submitted_||pending_)return false;
        if(api_.reset(device_,1,&fence_)!=VK_SUCCESS)return false;
        submitted_=false;return true;
    }
    const OwnedEyeResource* eye(unsigned index)const{return ready_&&index<2?&eyes_[index]:nullptr;}
    uint64_t allocated_bytes()const{return bytes_;}
    bool pending()const{return pending_;}
};
}
