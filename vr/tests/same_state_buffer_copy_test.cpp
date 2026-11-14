// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "same_state_buffer_copy.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T h(uint64_t x){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(x));else return T(x);}
int barriers=0,copies=0;
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags src,VkPipelineStageFlags dst,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t n,const VkBufferMemoryBarrier* b,uint32_t count,const VkImageMemoryBarrier* images){
    check(count==1&&images[0].image==h<VkImage>(3),"only acquired target image transitioned");
    if(!barriers){check(src==VK_PIPELINE_STAGE_ALL_COMMANDS_BIT&&dst==VK_PIPELINE_STAGE_TRANSFER_BIT&&n==1&&b[0].buffer==h<VkBuffer>(2)&&b[0].offset==0&&b[0].size==128,"entire exact pair ordered before read");check(images[0].oldLayout==VK_IMAGE_LAYOUT_UNDEFINED&&images[0].newLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,"discard only waited target content");}
    else check(src==VK_PIPELINE_STAGE_TRANSFER_BIT&&dst==VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT&&n==0&&images[0].newLayout==VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,"runtime target receives required final layout");++barriers;
}
VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer,VkBuffer buffer,VkImage image,VkImageLayout layout,uint32_t n,const VkBufferImageCopy* r){check(buffer==h<VkBuffer>(2)&&image==h<VkImage>(3)&&layout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&n==2,"one buffer-to-image call carries both eyes");for(unsigned eye=0;eye<2;++eye)check(r[eye].bufferOffset==eye*64&&r[eye].imageOffset.x==int(eye*4)&&r[eye].imageExtent.width==4&&r[eye].imageExtent.height==4&&r[eye].imageExtent.depth==1&&r[eye].bufferRowLength==0,"exact tightly packed eye regions");++copies;}
int main(){try{
    const SameStateBufferPair good{h<VkDevice>(1),h<VkBuffer>(2),128,4,5,6,7,8,4,4,3,VK_FORMAT_R8G8B8A8_SRGB};
    const SameStateBufferTarget target{h<VkDevice>(1),h<VkImage>(3),8,8,4,3,VK_FORMAT_R8G8B8A8_SRGB,true,true};
    auto reject=[&](auto source,auto out){barriers=copies=0;check(!record_same_state_buffer_copy(h<VkCommandBuffer>(4),7,source,out,barrier,copy)&&!barriers&&!copies,"invalid identity/extent records nothing");};
    auto s=good;s.recording=6;reject(s,target);s=good;s.generation=9;reject(s,target);s=good;s.state=0;reject(s,target);s=good;s.pose_sample=0;reject(s,target);s=good;s.bytes=127;reject(s,target);s=good;s.bytes=UINT64_MAX;reject(s,target);s=good;s.width=4097;reject(s,target);s=good;s.height=0;reject(s,target);s=good;s.queue_family=4;reject(s,target);s=good;s.format=VK_FORMAT_B8G8R8A8_UNORM;reject(s,target);
    s=good;s.queue_family=VK_QUEUE_FAMILY_IGNORED;auto ignored=target;ignored.queue_family=VK_QUEUE_FAMILY_IGNORED;reject(s,ignored);
    auto t=target;t.acquired_and_waited=false;reject(good,t);t=target;t.nonaliasing_proven=false;reject(good,t);t=target;t.device=h<VkDevice>(9);reject(good,t);t=target;t.width=7;reject(good,t);t=target;t.image=VK_NULL_HANDLE;reject(good,t);
    barriers=copies=0;check(record_same_state_buffer_copy(h<VkCommandBuffer>(4),7,good,target,barrier,copy)&&barriers==2&&copies==1,"complete same-state pair copied directly with no CPU pixels");
    std::cout<<"Direct GPU buffer pair copy admits one scoped pair and rejects invalid acquire/device/generation/range before commands\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
