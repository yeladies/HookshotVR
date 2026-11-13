// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_baseline_recorder.h"
#include "xr_uniform_copy_source.h"
#include <map>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <atomic>
using VmaAllocation=void*;
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
template<class T>T handle(uint64_t x){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(x));else return T(x);}
namespace mkwvk {
struct {VkDevice device=handle<VkDevice>(1);} dev;
std::atomic<bool> par_active{false};
struct Fake {
    uint64_t next=100,completed=0,generation=1;int allocations=0,fail=0,throw_at=0,draws=0,pushes=0,work=0,ends=0,corrupt_draw=0;
    VkImage active=VK_NULL_HANDLE;VkBuffer last_ubo=VK_NULL_HANDLE;
    std::map<VkImage,std::vector<unsigned char>> images;std::map<VkImageView,VkImage> views;
    std::map<VkBuffer,std::vector<unsigned char>> buffers;std::map<VmaAllocation,uint64_t> memory;
} fake;
uint64_t vk_timeline_completed(){return fake.completed;}
void note_work(){++fake.work;}
void end_pass(VkCommandBuffer){++fake.ends;}
void invalidate_mapped(VmaAllocation,uint64_t){}
uint64_t allocation_size(VmaAllocation m){return fake.memory.at(m);}
bool fail(){++fake.allocations;if(fake.allocations==fake.throw_at)throw std::bad_alloc();return fake.allocations==fake.fail;}
bool create_xr_private_image(const VkImageCreateInfo&,VkImage& image,VmaAllocation& memory){if(fail())return false;image=handle<VkImage>(++fake.next);memory=handle<VmaAllocation>(++fake.next);fake.images[image].resize(16);fake.memory[memory]=16;return true;}
void destroy_image(VkImage image,VmaAllocation memory){for(auto entry:fake.views)check(entry.second!=image,"view must retire before image");check(fake.memory.erase(memory)==1,"owned image allocation");fake.images.erase(image);}
bool create_buffer(uint64_t n,VkBufferUsageFlags,bool,VkBuffer& buffer,VmaAllocation& memory,void** mapped,bool){if(fail())return false;buffer=handle<VkBuffer>(++fake.next);memory=handle<VmaAllocation>(++fake.next);fake.buffers[buffer].resize(n);fake.memory[memory]=n;if(mapped)*mapped=fake.buffers[buffer].data();return true;}
bool create_device_buffer(uint64_t n,VkBufferUsageFlags flags,VkBuffer& b,VmaAllocation& m,bool strict){check(strict,"private buffer must be strict");return create_buffer(n,flags,false,b,m,nullptr,true);}
void destroy_buffer(VkBuffer b,VmaAllocation m){check(fake.memory.erase(m)==1,"owned buffer allocation");fake.buffers.erase(b);}
bool memory_xr_uniform_copy_source(VkBuffer b,uint64_t recording,XrUniformCopySource& out){if(b!=handle<VkBuffer>(10))return false;out={b,560,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,XrUniformSourceKind::Mirror,{recording,fake.generation,1}};return true;}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice,const VkImageCreateInfo*,const VkAllocationCallbacks*,VkImage* out){*out=handle<VkImage>(++fake.next);return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice,VkImage,const VkAllocationCallbacks*){}
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice,VkImage,VkMemoryRequirements* out){*out={16,4,1};}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice,const VkImageViewCreateInfo* in,const VkAllocationCallbacks*,VkImageView* out){*out=handle<VkImageView>(++fake.next);fake.views[*out]=in->image;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice,VkImageView view,const VkAllocationCallbacks*){check(fake.views.erase(view)==1,"owned image view");}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer,const VkDependencyInfo*){}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(VkCommandBuffer,VkImage src,VkImageLayout,VkImage dst,VkImageLayout,uint32_t,const VkImageCopy*){fake.images.at(dst)=fake.images.at(src);}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer src,VkBuffer dst,uint32_t,const VkBufferCopy* region){std::memcpy(fake.buffers.at(dst).data()+region->dstOffset,fake.buffers.at(src).data()+region->srcOffset,size_t(region->size));}
void vkCmdCopyImageToBuffer(VkCommandBuffer,VkImage image,VkImageLayout,VkBuffer buffer,uint32_t,const VkBufferImageCopy* region){std::memcpy(fake.buffers.at(buffer).data()+region->bufferOffset,fake.images.at(image).data(),16);}
void vkCmdBeginRendering(VkCommandBuffer,const VkRenderingInfo* rendering){check(rendering->layerCount==1&&rendering->colorAttachmentCount==1&&rendering->pDepthAttachment,"private color and depth rendering");fake.active=fake.views.at(rendering->pColorAttachments[0].imageView);}
void vkCmdEndRendering(VkCommandBuffer){}
void vkCmdDraw(VkCommandBuffer,uint32_t n,uint32_t instances,uint32_t first,uint32_t base){check(n==3&&instances==1&&first==0&&base==0,"exact native geometry");fake.images.at(fake.active)[0]=7;++fake.draws;if(fake.draws==fake.corrupt_draw)fake.images.at(fake.active)[1]=99;}
void vkCmdDrawIndexed(VkCommandBuffer c,uint32_t n,uint32_t i,uint32_t first,int32_t vertex,uint32_t base){check(vertex==-2,"signed indexed vertex offset");vkCmdDraw(c,n,i,first,base);}
VKAPI_ATTR void VKAPI_CALL push_descriptors(VkCommandBuffer,VkPipelineBindPoint,VkPipelineLayout,uint32_t,uint32_t count,const VkWriteDescriptorSet* writes){check(count==1,"full bounded descriptor set");fake.last_ubo=writes[0].pBufferInfo[0].buffer;++fake.pushes;}
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR=push_descriptors;
}
#include "baseline_recorder_function.inc"
using namespace mkwvk;
XrBaselineRecorderInput input(VkDescriptorBufferInfo& buffer){
    XrBaselineRecorderInput in;in.serial=1;in.recording=9;in.program=3;in.command=handle<VkCommandBuffer>(2);in.layout=handle<VkPipelineLayout>(4);
    in.color={handle<VkImage>(11),11,VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT,0,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,4,1,1,1};
    in.depth=in.color;in.depth.image=handle<VkImage>(12);in.depth.identity=12;in.depth.format=VK_FORMAT_D32_SFLOAT;in.depth.aspect=VK_IMAGE_ASPECT_DEPTH_BIT;in.depth.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    buffer={handle<VkBuffer>(10),0,560};in.camera=buffer;in.camera_slots={{0,0}};in.geometry={false,3,1,0,0,0};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffer;in.writes={write};return in;
}
void reset(){fake={};fake.images[handle<VkImage>(11)].resize(16);fake.images[handle<VkImage>(12)].resize(16);fake.buffers[handle<VkBuffer>(10)].resize(560,23);}
void cleaned(){check(fake.memory.empty()&&fake.views.empty()&&fake.images.size()==2&&fake.buffers.size()==1,"only original objects remain");}
int main(){try{
    VkDescriptorBufferInfo buffer;XrBaselineComparison comparison;
    for(int failure=1;failure<=7;++failure){reset();auto in=input(buffer);fake.fail=failure;auto* p=xr_baseline_recorder_create(in);check(!p,"partial allocation rejects");cleaned();check(!fake.draws&&!fake.pushes&&!fake.ends,"allocation failures record no work");}
    reset();auto in=input(buffer);fake.throw_at=5;check(!xr_baseline_recorder_create(in),"allocation exception contained");cleaned();
    reset();in=input(buffer);auto* p=xr_baseline_recorder_create(in);check(p,"allocated unchanged private targets");++fake.generation;check(!xr_baseline_recorder_prepare(p),"changed pool generation rejected");check(xr_baseline_recorder_retire(p,comparison)&&!p,"unsubmitted storage can unwind");cleaned();
    reset();in=input(buffer);p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepared snapshots");check(!fake.draws&&!fake.pushes,"prepare never draws or pushes");
    buffer.buffer=handle<VkBuffer>(99);check(!xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes)&&!fake.draws&&!fake.pushes,"changed source descriptor abandons before replay");
    check(!xr_baseline_recorder_retire(p,comparison)&&p,"abandoned submitted storage retained");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison)&&!p&&comparison.result==XrBaselineComparison::Result::Invalid,"completed abandoned storage released without result");cleaned();
    for(bool indexed:{false,true}){reset();in=input(buffer);in.geometry.indexed=indexed;in.geometry.vertex_offset=indexed?-2:0;p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepare exact source");fake.images[handle<VkImage>(11)][0]=7;check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes),"same original draw finishes");check(fake.draws==2&&fake.pushes==3&&fake.last_ubo==handle<VkBuffer>(10),"two private draws and explicit complete source restore");check(!xr_baseline_recorder_retire(p,comparison),"no early GPU retirement");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison)&&!p&&comparison.result==XrBaselineComparison::Result::Match&&comparison.changed_pixels==1,"four complete frames compare after known completion");cleaned();check(fake.buffers.at(handle<VkBuffer>(10))==std::vector<unsigned char>(560,23),"source uniform payload unchanged");}
    // A second view begins inside the first and extends the copied camera span.
    reset();in=input(buffer);VkDescriptorBufferInfo aliases[2]={{handle<VkBuffer>(10),0,256},{handle<VkBuffer>(10),128,432}};
    in.camera=aliases[0];in.writes[0].descriptorCount=2;in.writes[0].pBufferInfo=aliases;
    p=xr_baseline_recorder_create(in);check(p&&p->source.camera.range==560&&p->source.camera_slots.size()==2,"transitive camera alias closure");
    for(unsigned eye=0;eye<2;++eye){const auto* b=p->descriptors[eye].writes()[0].pBufferInfo;
        check(b[0].buffer==b[1].buffer&&b[0].offset==0&&b[0].range==256&&b[1].offset==128&&b[1].range==432,"private offsets and individual ranges preserved");}
    check(xr_baseline_recorder_prepare(p),"prepare offset aliases");
    for(const auto b:p->uniforms)check(fake.buffers.at(b)==fake.buffers.at(handle<VkBuffer>(10)),"entire camera union copied unchanged");
    fake.images[handle<VkImage>(11)][0]=7;check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes),"replay offset aliases");
    fake.completed=9;check(xr_baseline_recorder_retire(p,comparison)&&comparison.result==XrBaselineComparison::Result::Match,"offset alias baseline matches");cleaned();
    // Reverse-ordered aliases require another pass and preserve a nonzero base.
    reset();in=input(buffer);VkDescriptorBufferInfo chain[3]={{handle<VkBuffer>(10),384,176},{handle<VkBuffer>(10),128,256},{handle<VkBuffer>(10),256,256}};
    in.camera=chain[1];in.camera_slots={{0,1}};in.writes[0].descriptorCount=3;in.writes[0].pBufferInfo=chain;
    p=xr_baseline_recorder_create(in);check(p&&p->source.camera.offset==128&&p->source.camera.range==432&&p->source.camera_slots.size()==3,"reverse chain closes with nonzero copy base");
    const auto* relative=p->descriptors[0].writes()[0].pBufferInfo;
    check(relative[0].offset==256&&relative[1].offset==0&&relative[2].offset==128,"rebase all aliases relative to copy origin");
    check(xr_baseline_recorder_retire(p,comparison),"unsubmitted chain releases");cleaned();
    reset();in=input(buffer);VkDescriptorBufferInfo excessive[2]={buffer,{handle<VkBuffer>(10),128,65536}};
    in.writes[0].descriptorCount=2;in.writes[0].pBufferInfo=excessive;
    check(!xr_baseline_recorder_create(in)&&!fake.allocations,"alias union budget rejects before allocation");cleaned();
    reset();in=input(buffer);VkDescriptorBufferInfo overflow[2]={buffer,{handle<VkBuffer>(10),UINT64_MAX-3,8}};
    in.writes[0].descriptorCount=2;in.writes[0].pBufferInfo=overflow;
    check(!xr_baseline_recorder_create(in)&&!fake.allocations,"overflow alias rejected before arithmetic");cleaned();
    reset();in=input(buffer);VkDescriptorBufferInfo storage=buffer;auto extra=in.writes[0];extra.dstBinding=1;extra.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;extra.pBufferInfo=&storage;in.writes.push_back(extra);
    check(!xr_baseline_recorder_create(in)&&!fake.allocations,"overlapping storage camera alias rejected before allocation");cleaned();
    reset();in=input(buffer);p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepare negative readback");fake.images[handle<VkImage>(11)][0]=7;fake.corrupt_draw=2;
    check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes),"record deliberately divergent private right");fake.completed=9;
    check(xr_baseline_recorder_retire(p,comparison)&&comparison.result==XrBaselineComparison::Result::Mismatch&&comparison.mismatched_pixels[1]==1,"actual recorder readback catches private mismatch");cleaned();
    reset();in=input(buffer);p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepare skipped original");
    check(!xr_baseline_recorder_finish(p,2,9,3,in.command,in.writes)&&fake.draws==0,"next serial never replays a skipped source");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison),"skipped source retires after completion");cleaned();
    std::cout<<"Actual recorder unwinds allocation failures, rejects stale state, restores original descriptors and retires only completed private work\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
