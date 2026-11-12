// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_eye_image_factory.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>
using namespace mkwvk;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T handle(uint64_t id){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(id));else return T(id);}
struct Fake {int probes=0,probe_frees=0,allocates=0,releases=0,copies=0,barriers=0,fail_at=0;uint64_t requirement=1024;EyeImageSource source;std::map<uintptr_t,VkImage> live;} fake;
VKAPI_ATTR VkResult VKAPI_CALL raw(VkDevice,const VkImageCreateInfo* ci,const VkAllocationCallbacks*,VkImage* out){++fake.probes;check(ci->format==fake.source.format&&ci->mipLevels==fake.source.levels&&ci->arrayLayers==fake.source.layers&&ci->flags==fake.source.flags&&ci->initialLayout==VK_IMAGE_LAYOUT_UNDEFINED,"clone preserves native shape/format/flags");*out=handle<VkImage>(1);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL raw_free(VkDevice,VkImage h,const VkAllocationCallbacks*){check(h==handle<VkImage>(1),"only unbound preflight image destroyed");++fake.probe_frees;}
VKAPI_ATTR void VKAPI_CALL requirements(VkDevice,VkImage,VkMemoryRequirements* r){*r={fake.requirement,256,1};}
bool allocate(const VkImageCreateInfo&,VkImage& image,uintptr_t& allocation){++fake.allocates;if(fake.allocates==fake.fail_at)return false;allocation=100+fake.allocates;image=handle<VkImage>(200+fake.allocates);fake.live[allocation]=image;return true;}
void release(VkImage image,uintptr_t allocation){check(fake.live.count(allocation)&&fake.live[allocation]==image&&image!=fake.source.image,"only owned images released");fake.live.erase(allocation);++fake.releases;}
uint64_t allocation_bytes(uintptr_t){return fake.requirement;}
VKAPI_ATTR void VKAPI_CALL barrier(VkCommandBuffer,const VkDependencyInfo* d){++fake.barriers;if(d->imageMemoryBarrierCount){check(d->imageMemoryBarrierCount==3,"source plus two destination barriers");check(d->pImageMemoryBarriers[0].image==fake.source.image&&d->pImageMemoryBarriers[0].oldLayout==VK_IMAGE_LAYOUT_GENERAL&&d->pImageMemoryBarriers[0].newLayout==VK_IMAGE_LAYOUT_GENERAL,"original layout retained");}else check(d->memoryBarrierCount==1,"post-copy dependency");}
VKAPI_ATTR void VKAPI_CALL copy(VkCommandBuffer,VkImage source,VkImageLayout sl,VkImage destination,VkImageLayout dl,uint32_t n,const VkImageCopy* regions){
    ++fake.copies;check(source==fake.source.image&&destination!=source&&sl==VK_IMAGE_LAYOUT_GENERAL&&dl==sl,"copy only reads original");
    const unsigned aspects=fake.source.aspect==(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT)?2:1;
    check(n==aspects*fake.source.levels,"all levels/aspects copied");
    for(uint32_t i=0;i<n;++i){const auto& r=regions[i];check(r.srcSubresource.aspectMask==r.dstSubresource.aspectMask&&(r.srcSubresource.aspectMask&(r.srcSubresource.aspectMask-1))==0,"copy uses one aspect per region");check(r.srcSubresource.layerCount==fake.source.layers&&r.extent.width==std::max(1u,fake.source.width>>r.srcSubresource.mipLevel),"complete mip/layer extent");}
}
EyeImageFactoryApi api(){return {handle<VkDevice>(2),raw,raw_free,requirements,allocate,release,allocation_bytes,barrier,copy};}
EyeImageSource source(){EyeImageSource s;s.image=handle<VkImage>(99);s.identity=7;s.format=VK_FORMAT_B10G11R11_UFLOAT_PACK32;s.aspect=VK_IMAGE_ASPECT_COLOR_BIT;s.flags=VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;s.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;s.width=16;s.height=8;s.levels=4;s.layers=2;return s;}
int main(){try {
    for(bool depth:{false,true}){fake={};fake.source=source();if(depth){fake.source.format=VK_FORMAT_D24_UNORM_S8_UINT;fake.source.aspect=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;fake.source.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;}
        {EyeImageClonePair pair;check(pair.create(fake.source,api(),2048),"native clone pair");check(fake.probes==1&&fake.probe_frees==1&&pair.allocated_bytes()==2048&&pair.eye(0)!=pair.eye(1),"preflight and separate images");
        check(pair.record_initial_copy(handle<VkCommandBuffer>(3),10)&&fake.copies==2&&fake.barriers==2,"initial copies recorded");check(!pair.record_initial_copy(handle<VkCommandBuffer>(3),10),"cannot repeat undefined initialization");
        check(pair.mark_used(20)&&pair.mark_used(11)&&!pair.retire(19)&&fake.live.size()==2,"later use extends retirement monotonically");check(pair.retire(20)&&fake.live.empty(),"completed timeline permits release");check(pair.create(fake.source,api(),2048),"new generation can be created after explicit retirement");}
        check(fake.live.empty(),"unsubmitted next generation cleaned");
    }
    for(int fail:{1,2}){fake={};fake.source=source();fake.fail_at=fail;{EyeImageClonePair pair;check(!pair.create(fake.source,api(),2048)&&!pair.eye(0),"partial allocation rejected");}check(fake.live.empty()&&fake.releases==fail-1,"partial allocations unwind");}
    fake={};fake.source=source();{EyeImageClonePair pair;check(!pair.create(fake.source,api(),2047)&&fake.allocates==0,"budget rejected before allocation");}
    for(int invalid=0;invalid<4;++invalid){fake={};fake.source=source();if(invalid==0)fake.source.sparse=true;if(invalid==1)fake.source.levels=31;if(invalid==2)fake.source.aspect=VK_IMAGE_ASPECT_DEPTH_BIT;if(invalid==3)fake.source.samples=VK_SAMPLE_COUNT_4_BIT;EyeImageClonePair pair;check(!pair.create(fake.source,api(),4096)&&fake.probes==0,"unsupported shape rejected without GPU allocation");}
    fake={};fake.source=source();{EyeImageClonePair pair;check(pair.create(fake.source,api(),2048)&&pair.record_initial_copy(handle<VkCommandBuffer>(3),10),"pending pair");}check(fake.live.size()==2,"unknown completion retains images");
    std::cout<<"Strict image clones preserve shape, split depth/stencil copies and retire after last GPU use\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
