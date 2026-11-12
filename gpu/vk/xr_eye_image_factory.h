// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <algorithm>
#include <cstdint>
namespace mkwvk {
struct EyeImageSource {
    VkImage image=VK_NULL_HANDLE;uint64_t identity=0;
    VkFormat format=VK_FORMAT_UNDEFINED;VkImageAspectFlags aspect=0;
    VkImageCreateFlags flags=0;VkImageUsageFlags usage=0;
    uint32_t width=0,height=0,levels=0,layers=0;
    bool sparse=false;VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT;
    VkImageType type=VK_IMAGE_TYPE_2D;
};
struct EyeImageFactoryApi {
    VkDevice device=VK_NULL_HANDLE;
    PFN_vkCreateImage create_raw=nullptr;PFN_vkDestroyImage destroy_raw=nullptr;
    PFN_vkGetImageMemoryRequirements requirements=nullptr;
    bool (*allocate_strict)(const VkImageCreateInfo&,VkImage&,uintptr_t&)=nullptr;
    void (*release)(VkImage,uintptr_t)=nullptr;uint64_t (*allocation_bytes)(uintptr_t)=nullptr;
    PFN_vkCmdPipelineBarrier2 barrier=nullptr;PFN_vkCmdCopyImage copy=nullptr;
    bool valid()const{return device&&create_raw&&destroy_raw&&requirements&&allocate_strict&&release&&allocation_bytes&&barrier&&copy;}
};
inline VkImageAspectFlags eye_format_aspects(VkFormat format) {
    switch(format){
    case VK_FORMAT_D16_UNORM:case VK_FORMAT_X8_D24_UNORM_PACK32:case VK_FORMAT_D32_SFLOAT:return VK_IMAGE_ASPECT_DEPTH_BIT;
    case VK_FORMAT_S8_UINT:return VK_IMAGE_ASPECT_STENCIL_BIT;
    case VK_FORMAT_D16_UNORM_S8_UINT:case VK_FORMAT_D24_UNORM_S8_UINT:case VK_FORMAT_D32_SFLOAT_S8_UINT:return VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    default:return format>VK_FORMAT_UNDEFINED&&format<=VK_FORMAT_ASTC_12x12_SRGB_BLOCK?VK_IMAGE_ASPECT_COLOR_BIT:0;
    }
}
// Strict, separate native images; never a shallow Texture copy or a guest registration.
// No views or descriptor remapping yet. The caller owns device/allocator lifetime and
// must retire only using the actual completed timeline of the queue that used these images.
class EyeImageClonePair {
    EyeImageFactoryApi api_{};EyeImageSource source_{};
    std::array<VkImage,2> images_{};std::array<uintptr_t,2> allocations_{};
    uint64_t bytes_=0,last_use_=0;bool ready_=false,recorded_=false;
    void release() {
        for(unsigned i=0;i<2;++i){if(images_[i])api_.release(images_[i],allocations_[i]);images_[i]=VK_NULL_HANDLE;allocations_[i]=0;}
        bytes_=0;ready_=false;recorded_=false;
    }
public:
    EyeImageClonePair()=default;EyeImageClonePair(const EyeImageClonePair&)=delete;EyeImageClonePair& operator=(const EyeImageClonePair&)=delete;
    ~EyeImageClonePair(){if(!last_use_)release();} // Unretired submitted storage is retained, never freed speculatively.
    bool create(const EyeImageSource& source,const EyeImageFactoryApi& api,uint64_t budget) {
        const VkImageCreateFlags supported=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT|VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT|
            VK_IMAGE_CREATE_BLOCK_TEXEL_VIEW_COMPATIBLE_BIT|VK_IMAGE_CREATE_EXTENDED_USAGE_BIT|VK_IMAGE_CREATE_ALIAS_BIT;
        const VkImageUsageFlags transfer=VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if(ready_||last_use_||!api.valid()||!source.image||!source.identity||source.sparse||source.type!=VK_IMAGE_TYPE_2D||
           source.samples!=VK_SAMPLE_COUNT_1_BIT||!source.width||!source.height||source.width>16384||source.height>16384||
           !source.layers||source.layers>2048||!source.levels||source.levels>32||!source.aspect||
           source.aspect!=eye_format_aspects(source.format)||(source.flags&~supported)||
           (source.usage&transfer)!=transfer||!budget||budget>512ull*1024*1024)return false;
        uint32_t levels=1;for(uint32_t extent=std::max(source.width,source.height);extent>1;extent>>=1)++levels;
        if(source.levels>levels||((source.flags&VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)&&
           (source.width!=source.height||source.layers<6)))return false;
        api_=api;source_=source;
        VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ci.flags=source.flags;ci.imageType=source.type;ci.format=source.format;
        ci.extent={source.width,source.height,1};ci.mipLevels=source.levels;ci.arrayLayers=source.layers;ci.samples=source.samples;
        ci.tiling=VK_IMAGE_TILING_OPTIMAL;ci.usage=source.usage;ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
        // Query the driver's real requirements before allocating either eye.
        VkImage probe=VK_NULL_HANDLE;if(api_.create_raw(api_.device,&ci,nullptr,&probe)!=VK_SUCCESS)return false;
        VkMemoryRequirements requirements{};api_.requirements(api_.device,probe,&requirements);api_.destroy_raw(api_.device,probe,nullptr);
        if(!requirements.size||requirements.size>budget/2)return false;
        for(unsigned i=0;i<2;++i){
            if(!api_.allocate_strict(ci,images_[i],allocations_[i])){release();return false;}
            const auto size=api_.allocation_bytes(allocations_[i]);
            if(!size||size!=requirements.size||size>budget-bytes_){release();return false;}bytes_+=size;
        }
        ready_=true;return true;
    }
    bool record_initial_copy(VkCommandBuffer command,uint64_t value) {
        if(!ready_||recorded_||!command||!value)return false;
        std::array<VkImageMemoryBarrier2,3> barriers{};
        for(unsigned i=0;i<3;++i){auto& b=barriers[i];b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            b.srcStageMask=i?VK_PIPELINE_STAGE_2_NONE:VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            b.srcAccessMask=i?0:VK_ACCESS_2_MEMORY_WRITE_BIT;b.dstStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;
            b.dstAccessMask=i?VK_ACCESS_2_TRANSFER_WRITE_BIT:VK_ACCESS_2_TRANSFER_READ_BIT;
            b.oldLayout=i?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_GENERAL;b.newLayout=VK_IMAGE_LAYOUT_GENERAL;
            b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i?images_[i-1]:source_.image;
            b.subresourceRange={source_.aspect,0,source_.levels,0,source_.layers};}
        VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.imageMemoryBarrierCount=3;dependency.pImageMemoryBarriers=barriers.data();
        std::array<VkImageCopy,64> regions{};uint32_t count=0;
        for(auto aspect:{VK_IMAGE_ASPECT_COLOR_BIT,VK_IMAGE_ASPECT_DEPTH_BIT,VK_IMAGE_ASPECT_STENCIL_BIT})if(source_.aspect&aspect)
            for(uint32_t level=0;level<source_.levels;++level){auto& r=regions[count++];r.srcSubresource=r.dstSubresource={VkImageAspectFlags(aspect),level,0,source_.layers};
                r.extent={std::max(1u,source_.width>>level),std::max(1u,source_.height>>level),1};}
        last_use_=value;recorded_=true;
        api_.barrier(command,&dependency);
        for(auto image:images_)api_.copy(command,source_.image,VK_IMAGE_LAYOUT_GENERAL,image,VK_IMAGE_LAYOUT_GENERAL,count,regions.data());
        VkMemoryBarrier2 after{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};after.srcStageMask=VK_PIPELINE_STAGE_2_COPY_BIT;
        after.srcAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT|VK_ACCESS_2_TRANSFER_WRITE_BIT;
        after.dstStageMask=VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;after.dstAccessMask=VK_ACCESS_2_MEMORY_READ_BIT|VK_ACCESS_2_MEMORY_WRITE_BIT;
        dependency.imageMemoryBarrierCount=0;dependency.pImageMemoryBarriers=nullptr;dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&after;
        api_.barrier(command,&dependency);return true;
    }
    bool mark_used(uint64_t value){if(!ready_||!recorded_||!value)return false;last_use_=std::max(last_use_,value);return true;}
    bool retire(uint64_t completed){if(last_use_&&completed<last_use_)return false;release();last_use_=0;return true;}
    VkImage eye(unsigned index)const{return ready_&&index<2?images_[index]:VK_NULL_HANDLE;}
    uint64_t allocated_bytes()const{return bytes_;}
};
}
