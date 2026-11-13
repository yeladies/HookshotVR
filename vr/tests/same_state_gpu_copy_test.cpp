// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "same_state_gpu_copy.h"
#include <stdexcept>
#include <iostream>
using namespace hookshot::vr;
int barriers=0,copies=0;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t n,const VkImageMemoryBarrier* b){
    ++barriers;check(n==(barriers==1?3u:1u),"barrier count");
    check(b[n-1].newLayout==(barriers==1?VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),"target layout");
}
VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer,VkImage src,VkImageLayout sl,VkImage dst,VkImageLayout dl,uint32_t n,const VkImageCopy* r){
    check(src==(VkImage)(uintptr_t)(copies+1)&&dst==(VkImage)(uintptr_t)3,"eye routing");
    check(sl==VK_IMAGE_LAYOUT_GENERAL&&dl==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL&&n==1,"copy layouts");
    check(r->dstOffset.x==copies*16&&r->extent.width==16&&r->extent.height==8,"separate target rectangles");++copies;
}
int main(){try{
    std::array<SameStateEyeImage,2> eyes{};
    for(unsigned i=0;i<2;++i)eyes[i]={(VkImage)(uintptr_t)(i+1),7,8,9,10,i,16,8,0,VK_FORMAT_R8G8B8A8_UNORM};
    SameStateCopyTarget target{(VkImage)(uintptr_t)3,32,8,0,VK_FORMAT_R8G8B8A8_UNORM,true,true};
    const auto command=(VkCommandBuffer)(uintptr_t)4;
    auto run=[&](auto e,auto t,uint64_t recording=10){barriers=copies=0;return record_same_state_eye_copy(command,recording,e,t,barrier,copy);};
    check(run(eyes,target)&&barriers==2&&copies==2,"same-state pair copied without CPU pixel access");
    auto reject=[&](auto e,auto t){check(!run(e,t)&&barriers==0&&copies==0,"reject before any command");};
    for(unsigned which=0;which<2;++which){auto e=eyes;e[which].state++;reject(e,target);e=eyes;e[which].origin++;reject(e,target);e=eyes;e[which].pose_sample++;reject(e,target);e=eyes;e[which].recording++;reject(e,target);e=eyes;e[which].width++;reject(e,target);e=eyes;e[which].eye^=1;reject(e,target);e=eyes;e[which].queue_family++;reject(e,target);}
    auto e=eyes;e[1].image=e[0].image;reject(e,target);e=eyes;e[0].image=target.image;reject(e,target);
    auto t=target;t.acquired_and_waited=false;reject(eyes,t);t=target;t.nonaliasing_proven=false;reject(eyes,t);t=target;t.width++;reject(eyes,t);t=target;t.format=VK_FORMAT_B8G8R8A8_UNORM;reject(eyes,t);
    e=eyes;e[0].state=e[1].state=0;reject(e,target);e=eyes;e[0].pose_sample=e[1].pose_sample=0;reject(e,target);
    for(unsigned field=0;field<5;++field){e=eyes;t=target;
        if(field==0)e[0].image=VK_NULL_HANDLE;
        if(field==1)e[0].height=e[1].height=0;
        if(field==2)e[0].width=e[1].width=8193;
        if(field==3)e[0].origin=e[1].origin=0;
        if(field==4)t.image=VK_NULL_HANDLE;
        reject(e,t);
    }
    check(!run(eyes,target,11)&&!barriers&&!copies,"stale common recording rejected");
    barriers=copies=0;
    check(!record_same_state_eye_copy(VK_NULL_HANDLE,10,eyes,target,barrier,copy),"null command rejected");
    check(!record_same_state_eye_copy(command,10,eyes,target,nullptr,copy),"missing barrier rejected");
    check(!record_same_state_eye_copy(command,10,eyes,target,barrier,nullptr)&&!barriers&&!copies,"missing copy rejected before commands");
    e=eyes;e[0].format=e[1].format=VK_FORMAT_R8G8B8A8_SRGB;t=target;t.format=e[0].format;check(run(e,t),"exact sRGB format supported");
    std::cout<<"Same-state GPU copy guards and separate eye routing passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
