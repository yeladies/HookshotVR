// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace hookshot::vr {
// One completed-by-ordering compute job owns both tightly packed RGBA planes.
// The owner retains this buffer/source lease until its submission fence retires.
struct SameStateBufferPair {
    VkDevice device=VK_NULL_HANDLE;VkBuffer buffer=VK_NULL_HANDLE;
    uint64_t bytes=0,state=0,origin=0,pose_sample=0,recording=0,generation=0;
    uint32_t width=0,height=0,queue_family=0;VkFormat format=VK_FORMAT_UNDEFINED;
};
struct SameStateBufferTarget {
    VkDevice device=VK_NULL_HANDLE;VkImage image=VK_NULL_HANDLE;uint64_t generation=0;
    uint32_t width=0,height=0,queue_family=0;VkFormat format=VK_FORMAT_UNDEFINED;
    bool acquired_and_waited=false,nonaliasing_proven=false;
};
// Raw texel transport only: the producer must prepare the correct transfer
// function for UNORM/SRGB. No implicit gamma conversion or queue transfer occurs.
inline bool record_same_state_buffer_copy(VkCommandBuffer command,uint64_t recording,const SameStateBufferPair& source,
        const SameStateBufferTarget& target,PFN_vkCmdPipelineBarrier barrier,PFN_vkCmdCopyBufferToImage copy){
    if(!command||!recording||!barrier||!copy||!source.device||source.device!=target.device||!source.buffer||!target.image||
       !source.state||!source.origin||!source.pose_sample||source.recording!=recording||!source.generation||source.generation!=target.generation||
       !target.acquired_and_waited||!target.nonaliasing_proven||source.width<2||source.height<2||source.width>4096||source.height>4096||
       uint64_t(source.width)*source.height>8388608||source.queue_family>=VK_QUEUE_FAMILY_FOREIGN_EXT||source.queue_family!=target.queue_family||target.width!=2*source.width||target.height!=source.height||
       source.format!=target.format||(source.format!=VK_FORMAT_R8G8B8A8_UNORM&&source.format!=VK_FORMAT_R8G8B8A8_SRGB))return false;
    const uint64_t eye_bytes=uint64_t(source.width)*source.height*4,pair_bytes=2*eye_bytes;
    if(source.bytes<pair_bytes||source.bytes>64ull*1024*1024)return false;
    VkBufferMemoryBarrier ready{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};ready.buffer=source.buffer;ready.size=pair_bytes;
    ready.srcQueueFamilyIndex=ready.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;ready.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    image.image=target.image;image.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};image.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&ready,1,&image);
    VkBufferImageCopy regions[2]{};for(unsigned eye=0;eye<2;++eye){auto& r=regions[eye];r.bufferOffset=eye*eye_bytes;r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};r.imageOffset.x=int32_t(eye*source.width);r.imageExtent={source.width,source.height,1};}
    copy(command,source.buffer,target.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,regions);
    image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;image.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&image);return true;
}
}
