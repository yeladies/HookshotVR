// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
namespace hookshot::vr {
// Internal, single-queue recording contract. The caller owns all images and must
// prove nonaliasing, usage/format features and lifetime through queue completion.
// Identity is producer provenance, not proof that a scene was rendered correctly.
struct SameStateEyeImage {
    VkImage image=VK_NULL_HANDLE;
    uint64_t state=0,origin=0,pose_sample=0,recording=0;
    uint32_t eye=0,width=0,height=0,queue_family=0;
    VkFormat format=VK_FORMAT_UNDEFINED;
};
struct SameStateCopyTarget {
    VkImage image=VK_NULL_HANDLE;
    uint32_t width=0,height=0,queue_family=0;
    VkFormat format=VK_FORMAT_UNDEFINED;
    bool acquired_and_waited=false,nonaliasing_proven=false;
};
inline bool record_same_state_eye_copy(VkCommandBuffer command,uint64_t recording,
        const std::array<SameStateEyeImage,2>& eyes,const SameStateCopyTarget& target,
        PFN_vkCmdPipelineBarrier barrier,PFN_vkCmdCopyImage copy) {
    const auto& a=eyes[0];const auto& b=eyes[1];
    if(!command||!recording||!barrier||!copy||!target.image||!target.acquired_and_waited||!target.nonaliasing_proven||
       !a.image||!b.image||a.image==b.image||a.image==target.image||b.image==target.image||
       !a.state||!a.origin||!a.pose_sample||a.state!=b.state||a.origin!=b.origin||a.pose_sample!=b.pose_sample||
       a.recording!=recording||b.recording!=recording||a.eye!=0||b.eye!=1||
       !a.width||!a.height||a.width>8192||a.height>8192||a.width!=b.width||a.height!=b.height||
       target.width!=2*a.width||target.height!=a.height||a.queue_family!=b.queue_family||a.queue_family!=target.queue_family||
       a.format!=b.format||a.format!=target.format||
       (a.format!=VK_FORMAT_R8G8B8A8_UNORM&&a.format!=VK_FORMAT_R8G8B8A8_SRGB))return false;
    // Sources must already be GENERAL on this same queue. No queue ownership
    // transfer or guessed layouts; source images remain GENERAL after copying.
    std::array<VkImageMemoryBarrier,3> images{};
    for(unsigned i=0;i<3;++i){auto& v=images[i];v.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        v.srcQueueFamilyIndex=v.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
        v.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        v.image=i<2?eyes[i].image:target.image;
        v.oldLayout=i<2?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;
        v.newLayout=i<2?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        v.srcAccessMask=i<2?VK_ACCESS_MEMORY_WRITE_BIT:0;
        v.dstAccessMask=i<2?VK_ACCESS_TRANSFER_READ_BIT:VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    barrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,3,images.data());
    for(unsigned i=0;i<2;++i){VkImageCopy region{};region.srcSubresource=region.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
        region.extent={a.width,a.height,1};region.dstOffset.x=int32_t(i*a.width);
        copy(command,eyes[i].image,VK_IMAGE_LAYOUT_GENERAL,target.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&region);
    }
    VkImageMemoryBarrier done=images[2];done.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;done.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    done.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;done.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,0,0,nullptr,0,nullptr,1,&done);
    return true;
}
}
