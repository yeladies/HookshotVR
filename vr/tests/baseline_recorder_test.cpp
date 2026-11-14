// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_baseline_recorder.h"
#include "xr_uniform_copy_source.h"
#include "xr_matrix_patch_check.h"
#include "xr_shifted_draw_check.h"
#include "xr_camera_payload_capture.h"
#include "xr_eye_matrix_patch_spv.h"
#include <set>
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
    int matrix_dispatches=0;bool matrix_corrupt=false,matrix_touch_source=false,large_scene=false,wrong_shift=false;
    struct MatrixParameters {VkDeviceAddress source=0,destination=0;uint32_t bytes=0;float shift=0;uint32_t mode=0,reserved=0;} matrix;
    std::set<VkShaderModule> shaders;std::set<VkPipelineLayout> layouts;std::set<VkPipeline> pipelines;
    VkImage active_depth=VK_NULL_HANDLE;VkImage active=VK_NULL_HANDLE;VkBuffer last_ubo=VK_NULL_HANDLE;
    std::map<VkImage,uint64_t> raw_sizes;
    std::map<VkImage,std::vector<unsigned char>> images;std::map<VkImageView,VkImage> views;
    std::map<VkBuffer,std::vector<unsigned char>> buffers;std::map<VmaAllocation,uint64_t> memory;
} fake;
uint64_t vk_timeline_completed(){return fake.completed;}
void note_work(){++fake.work;}
void end_pass(VkCommandBuffer){++fake.ends;}
void invalidate_mapped(VmaAllocation,uint64_t){}
uint64_t allocation_size(VmaAllocation m){return fake.memory.at(m);}
bool fail(){++fake.allocations;if(fake.allocations==fake.throw_at)throw std::bad_alloc();return fake.allocations==fake.fail;}
bool create_xr_private_image(const VkImageCreateInfo& ci,VkImage& image,VmaAllocation& memory){if(fail())return false;image=handle<VkImage>(++fake.next);memory=handle<VmaAllocation>(++fake.next);const auto n=uint64_t(ci.extent.width)*ci.extent.height*4;fake.images[image].resize(n);fake.memory[memory]=n;return true;}
void destroy_image(VkImage image,VmaAllocation memory){for(auto entry:fake.views)check(entry.second!=image,"view must retire before image");check(fake.memory.erase(memory)==1,"owned image allocation");fake.images.erase(image);}
bool create_buffer(uint64_t n,VkBufferUsageFlags,bool,VkBuffer& buffer,VmaAllocation& memory,void** mapped,bool){if(fail())return false;buffer=handle<VkBuffer>(++fake.next);memory=handle<VmaAllocation>(++fake.next);fake.buffers[buffer].resize(n);fake.memory[memory]=n;if(mapped)*mapped=fake.buffers[buffer].data();return true;}
bool create_device_buffer(uint64_t n,VkBufferUsageFlags flags,VkBuffer& b,VmaAllocation& m,bool strict){check(strict,"private buffer must be strict");return create_buffer(n,flags,false,b,m,nullptr,true);}
void destroy_buffer(VkBuffer b,VmaAllocation m){check(fake.memory.erase(m)==1,"owned buffer allocation");fake.buffers.erase(b);}
XrRootAliasProbe* xr_root_alias_probe_create(const XrRootAliasProbeInput&){return nullptr;}
bool xr_root_alias_probe_record(XrRootAliasProbe*){return false;}
bool xr_root_alias_probe_indirect_command(XrRootAliasProbe*,VkCommandBuffer,uint64_t,unsigned,XrRootIndirectCommand&){return false;}
bool xr_root_alias_probe_retire(XrRootAliasProbe*& p,uint64_t,const std::array<const unsigned char*,2>&,size_t,XrRootAliasProbeResult&){check(!p,"default-off fixture must not borrow a root probe");return true;}
bool memory_xr_uniform_copy_source(VkBuffer b,uint64_t recording,XrUniformCopySource& out){if(b!=handle<VkBuffer>(10))return false;out={b,560,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,XrUniformSourceKind::Mirror,{recording,fake.generation,1}};return true;}
VkDeviceAddress vkGetBufferDeviceAddress(VkDevice,const VkBufferDeviceAddressInfo* info){return uint64_t(reinterpret_cast<uintptr_t>(info->buffer))<<20;}
VkResult vkCreateShaderModule(VkDevice,const VkShaderModuleCreateInfo*,const VkAllocationCallbacks*,VkShaderModule* out){if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkShaderModule>(++fake.next);fake.shaders.insert(*out);return VK_SUCCESS;}
void vkDestroyShaderModule(VkDevice,VkShaderModule value,const VkAllocationCallbacks*){check(fake.shaders.erase(value)==1,"owned shader released");}
VkResult vkCreatePipelineLayout(VkDevice,const VkPipelineLayoutCreateInfo*,const VkAllocationCallbacks*,VkPipelineLayout* out){if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkPipelineLayout>(++fake.next);fake.layouts.insert(*out);return VK_SUCCESS;}
void vkDestroyPipelineLayout(VkDevice,VkPipelineLayout value,const VkAllocationCallbacks*){check(fake.layouts.erase(value)==1,"owned layout released");}
VkResult vkCreateComputePipelines(VkDevice,VkPipelineCache,uint32_t count,const VkComputePipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline* out){check(count==1,"one bounded patch pipeline");if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkPipeline>(++fake.next);fake.pipelines.insert(*out);return VK_SUCCESS;}
void vkDestroyPipeline(VkDevice,VkPipeline value,const VkAllocationCallbacks*){check(fake.pipelines.erase(value)==1,"owned pipeline released");}
void vkCmdBindPipeline(VkCommandBuffer,VkPipelineBindPoint point,VkPipeline pipeline){check(point==VK_PIPELINE_BIND_POINT_COMPUTE&&fake.pipelines.count(pipeline),"owned compute pipeline only");}
void vkCmdPushConstants(VkCommandBuffer,VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t bytes,const void* data){check(fake.layouts.count(layout)&&stages==VK_SHADER_STAGE_COMPUTE_BIT&&!offset&&bytes==32,"compute-only parameters");std::memcpy(&fake.matrix,data,32);}
// Emulate only transport for recorder lifecycle tests. The real shader is tested
// separately on GPU; this double cannot establish shader numerical correctness.
void vkCmdDispatch(VkCommandBuffer,uint32_t x,uint32_t y,uint32_t z){
    check(x==1&&y==1&&z==1,"one patch invocation");++fake.matrix_dispatches;
    const auto p=fake.matrix;auto& source=fake.buffers.at(handle<VkBuffer>(p.source>>20));auto& target=fake.buffers.at(handle<VkBuffer>(p.destination>>20));
    const size_t offset=p.source&((1u<<20)-1);check(offset+p.bytes<=source.size()&&p.bytes==target.size(),"bounded matrix source and destination");
    std::memcpy(target.data(),source.data()+offset,p.bytes);
    using namespace hookshot::vr;float base[56];std::memcpy(base,target.data(),sizeof base);EyeMatrices m{};
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)m.view.rows[r][c]=base[4*r+c];if(p.mode!=1)m.view.rows[0][3]=float(base[3]+p.shift);
    float view[12],inverse[12];for(int i=0;i<12;++i)view[i]=float(m.view.rows[i/4][i%4]);uniform_detail::inverse(view,inverse);for(int r=0;r<3;++r)m.view.eye[r]=inverse[4*r+3];
    for(int i=0;i<16;++i)m.projection[i]=base[28+i];if(p.mode==1)m.projection[2]=float(base[30]+p.shift);for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)m.world_to_clip[4*r+c]+=m.projection[4*r+k]*(k==3?(c==3?1:0):m.view.rows[k][c]);
    std::vector<std::byte> result;if(prepare_eye_uniform_copy(target.data(),p.bytes,m,ClipDepth::MinusOneToOne,result))std::memcpy(target.data(),result.data(),p.bytes);
    if(fake.matrix_corrupt)target[0xe0]^=1;if(fake.matrix_touch_source&&fake.matrix_dispatches==1)source.back()^=1;
}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice,const VkImageCreateInfo* ci,const VkAllocationCallbacks*,VkImage* out){*out=handle<VkImage>(++fake.next);fake.raw_sizes[*out]=uint64_t(ci->extent.width)*ci->extent.height*4;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice,VkImage image,const VkAllocationCallbacks*){check(fake.raw_sizes.erase(image)==1,"raw probe released");}
VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice,VkImage image,VkMemoryRequirements* out){*out={fake.raw_sizes.at(image),4,1};}
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice,const VkImageViewCreateInfo* in,const VkAllocationCallbacks*,VkImageView* out){*out=handle<VkImageView>(++fake.next);fake.views[*out]=in->image;return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice,VkImageView view,const VkAllocationCallbacks*){check(fake.views.erase(view)==1,"owned image view");}
VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier2(VkCommandBuffer,const VkDependencyInfo*){}
VKAPI_ATTR void VKAPI_CALL vkCmdCopyImage(VkCommandBuffer,VkImage src,VkImageLayout,VkImage dst,VkImageLayout,uint32_t,const VkImageCopy*){fake.images.at(dst)=fake.images.at(src);}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer src,VkBuffer dst,uint32_t,const VkBufferCopy* region){std::memcpy(fake.buffers.at(dst).data()+region->dstOffset,fake.buffers.at(src).data()+region->srcOffset,size_t(region->size));}
void vkCmdCopyImageToBuffer(VkCommandBuffer,VkImage image,VkImageLayout,VkBuffer buffer,uint32_t,const VkBufferImageCopy* region){std::memcpy(fake.buffers.at(buffer).data()+region->bufferOffset,fake.images.at(image).data(),fake.images.at(image).size());}
void vkCmdBeginRendering(VkCommandBuffer,const VkRenderingInfo* rendering){check(rendering->layerCount==1&&rendering->colorAttachmentCount==1&&rendering->pDepthAttachment,"private color and depth rendering");fake.active=fake.views.at(rendering->pColorAttachments[0].imageView);fake.active_depth=fake.views.at(rendering->pDepthAttachment->imageView);}
void vkCmdEndRendering(VkCommandBuffer){}
void large_draw(VkImage image,int shift){auto& pixels=fake.images.at(image);for(int y=8;y<12;++y)for(int x=70+shift;x<78+shift;++x)pixels[(y*160+x)*4]=7;}
void vkCmdDraw(VkCommandBuffer,uint32_t n,uint32_t instances,uint32_t first,uint32_t base){check(n==3&&instances==1&&first==0&&base==0,"exact native geometry");if(fake.large_scene){float p02=0;std::memcpy(&p02,fake.buffers.at(fake.last_ubo).data()+0x78,4);float tx=0;std::memcpy(&tx,fake.buffers.at(fake.last_ubo).data()+12,4);int shift=-int(std::round(p02*80))+int(std::round(tx*2));if(fake.wrong_shift)shift=-shift;large_draw(fake.active,shift);large_draw(fake.active_depth,shift);}else fake.images.at(fake.active)[0]=7;++fake.draws;if(fake.draws==fake.corrupt_draw)fake.images.at(fake.active)[1]=99;}
void vkCmdDrawIndexed(VkCommandBuffer c,uint32_t n,uint32_t i,uint32_t first,int32_t vertex,uint32_t base){check(vertex==-2,"signed indexed vertex offset");vkCmdDraw(c,n,i,first,base);}
void vkCmdDrawIndirect(VkCommandBuffer command,VkBuffer buffer,VkDeviceSize offset,uint32_t count,uint32_t stride){check(count==1&&stride==sizeof(VkDrawIndirectCommand),"single guarded draw");VkDrawIndirectCommand args{};std::memcpy(&args,fake.buffers.at(buffer).data()+offset,sizeof args);if(args.vertexCount)vkCmdDraw(command,args.vertexCount,args.instanceCount,args.firstVertex,args.firstInstance);}
void vkCmdDrawIndexedIndirect(VkCommandBuffer command,VkBuffer buffer,VkDeviceSize offset,uint32_t count,uint32_t stride){check(count==1&&stride==sizeof(VkDrawIndexedIndirectCommand),"single guarded indexed draw");VkDrawIndexedIndirectCommand args{};std::memcpy(&args,fake.buffers.at(buffer).data()+offset,sizeof args);if(args.indexCount)vkCmdDrawIndexed(command,args.indexCount,args.instanceCount,args.firstIndex,args.vertexOffset,args.firstInstance);}
VKAPI_ATTR void VKAPI_CALL push_descriptors(VkCommandBuffer,VkPipelineBindPoint,VkPipelineLayout,uint32_t,uint32_t count,const VkWriteDescriptorSet* writes){check(count==1,"full bounded descriptor set");fake.last_ubo=writes[0].pBufferInfo[0].buffer;++fake.pushes;}
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR=push_descriptors;
}
#include "baseline_recorder_function.inc"
using namespace mkwvk;
XrBaselineRecorderInput input(VkDescriptorBufferInfo& buffer){
    XrBaselineRecorderInput in;in.serial=1;in.recording=9;in.program=3;in.command=handle<VkCommandBuffer>(2);in.layout=handle<VkPipelineLayout>(4);
    in.color={handle<VkImage>(11),11,VK_FORMAT_B10G11R11_UFLOAT_PACK32,VK_IMAGE_ASPECT_COLOR_BIT,0,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,4,1,1,1};
    in.depth=in.color;in.depth.image=handle<VkImage>(12);in.depth.identity=12;in.depth.format=VK_FORMAT_D32_SFLOAT;in.depth.aspect=VK_IMAGE_ASPECT_DEPTH_BIT;in.depth.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    buffer={handle<VkBuffer>(10),0,560};in.camera=buffer;in.camera_object_bytes=560;in.camera_slots={{0,0}};in.geometry={false,3,1,0,0,0};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstBinding=0;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;write.pBufferInfo=&buffer;in.writes={write};return in;
}
void reset(){fake={};fake.images[handle<VkImage>(11)].resize(16);fake.images[handle<VkImage>(12)].resize(16);fake.buffers[handle<VkBuffer>(10)].resize(560,23);}
void cleaned(){check(fake.raw_sizes.empty()&&fake.shaders.empty()&&fake.layouts.empty()&&fake.pipelines.empty()&&fake.memory.empty()&&fake.views.empty()&&fake.images.size()==2&&fake.buffers.size()==1,"only original objects remain");}
int main(){try{
    VkDescriptorBufferInfo buffer;XrBaselineComparison comparison;
    for(int failure=1;failure<=7;++failure){reset();auto in=input(buffer);fake.fail=failure;auto* p=xr_baseline_recorder_create(in);check(!p,"partial allocation rejects");cleaned();check(!fake.draws&&!fake.pushes&&!fake.ends,"allocation failures record no work");}
    reset();auto in=input(buffer);fake.throw_at=5;check(!xr_baseline_recorder_create(in),"allocation exception contained");cleaned();
    reset();in=input(buffer);auto* p=xr_baseline_recorder_create(in);check(p,"allocated unchanged private targets");++fake.generation;check(!xr_baseline_recorder_prepare(p),"changed pool generation rejected");check(xr_baseline_recorder_retire(p,comparison)&&!p,"unsubmitted storage can unwind");cleaned();
    reset();in=input(buffer);p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepared snapshots");check(!fake.draws&&!fake.pushes,"prepare never draws or pushes");
    buffer.buffer=handle<VkBuffer>(99);check(!xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes)&&!fake.draws&&!fake.pushes,"changed source descriptor abandons before replay");
    check(!xr_baseline_recorder_retire(p,comparison)&&p,"abandoned submitted storage retained");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison)&&!p&&comparison.result==XrBaselineComparison::Result::Invalid,"completed abandoned storage released without result");cleaned();
    for(bool indexed:{false,true}){reset();in=input(buffer);in.geometry.indexed=indexed;in.geometry.vertex_offset=indexed?-2:0;p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepare exact source");fake.images[handle<VkImage>(11)][0]=7;check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes),"same original draw finishes");check(fake.draws==2&&fake.pushes==3&&fake.last_ubo==handle<VkBuffer>(10),"two private draws and explicit complete source restore");check(!xr_baseline_recorder_retire(p,comparison),"no early GPU retirement");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison)&&!p&&comparison.result==XrBaselineComparison::Result::Match&&comparison.changed_pixels==1,"four complete frames compare after known completion");cleaned();check(fake.buffers.at(handle<VkBuffer>(10))==std::vector<unsigned char>(560,23),"source uniform payload unchanged");}
    // Optional patches stay unbound to the unchanged baseline draws.
    for(uint64_t extent:{uint64_t(0),uint64_t(176),uint64_t(561),uint64_t(65540)}){reset();in=input(buffer);in.matrix_probe=true;in.camera_object_bytes=extent;check(!xr_baseline_recorder_create(in)&&!fake.allocations,"unproven or invalid full uniform extent rejected before allocation");cleaned();}
    for(int failure=1;failure<=12;++failure){reset();in=input(buffer);in.matrix_probe=true;fake.fail=failure;check(!xr_baseline_recorder_create(in),"matrix setup failure unwinds");cleaned();check(!fake.draws&&!fake.pushes&&!fake.ends&&!fake.matrix_dispatches,"failed optional setup records no commands");}
    for(int fault=0;fault<3;++fault){
        reset();in=input(buffer);in.matrix_probe=true;if(fault==0){in.camera.range=buffer.range=176;}float base[56]{};for(int i=0;i<3;++i){base[4*i+i]=1;base[44+4*i+i]=1;}
        base[28]=base[33]=1;base[38]=-(100.f+.05f)/(100.f-.05f);base[39]=-2.f*100.f*.05f/(100.f-.05f);base[42]=-1;for(int i=0;i<16;++i)base[12+i]=base[28+i];
        std::memcpy(fake.buffers.at(handle<VkBuffer>(10)).data(),base,sizeof base);const auto original=fake.buffers.at(handle<VkBuffer>(10));
        fake.matrix_corrupt=fault==1;fake.matrix_touch_source=fault==2;
        p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p)&&fake.matrix_dispatches==2,"same GPU source patched twice");
        fake.images[handle<VkImage>(11)][0]=7;check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes),"ordinary baseline finishes with patch diagnostic");
        check(fake.draws==2&&fake.last_ubo==handle<VkBuffer>(10),"patched buffers never bound to baseline draws, source restored");
        check(!xr_baseline_recorder_retire(p,comparison),"patch storage retained until completion");fake.completed=9;
        check(xr_baseline_recorder_retire(p,comparison)&&comparison.result==XrBaselineComparison::Result::Match,"unchanged image comparator stays strict");
        check(comparison.matrix_probe_requested&&comparison.matrix_payload_bytes==560&&comparison.matrix_probe_validated==(fault==0),"patch byte validation rejects corruption");
        if(fault!=2)check(comparison.matrix_source_unchanged&&fake.buffers.at(handle<VkBuffer>(10))==original,"source payload remains byte exact");
        if(fault==2)check(!comparison.matrix_source_unchanged,"source mutation detected");cleaned();
    }
    for(int failure=1;failure<=16;++failure){reset();in=input(buffer);in.matrix_probe=in.shifted_draw_probe=true;in.color.width=in.depth.width=160;in.color.height=in.depth.height=32;fake.fail=failure;
        check(!xr_baseline_recorder_create(in),"shifted setup failure unwinds every extra image, view, buffer and pipeline");cleaned();check(!fake.draws&&!fake.pushes&&!fake.ends&&!fake.matrix_dispatches,"shifted setup failure records no GPU work");}
    reset();in=input(buffer);in.shifted_draw_probe=true;check(!xr_baseline_recorder_create(in)&&!fake.allocations,"shifted draws require matrix validation");cleaned();
    // The lens experiment preserves two exact reference copies before two shifted draws.
    for(bool translated:{false,true})for(bool depth:{false,true})for(bool wrong:{false,true}){

        reset();in=input(buffer);in.matrix_probe=in.shifted_draw_probe=true;in.depth_reference=depth;in.view_shift_probe=translated;in.view_color_probe=translated&&!depth;in.view_shift_units=translated?2.f:.01f;in.color.width=in.depth.width=160;in.color.height=in.depth.height=32;
        fake.large_scene=true;fake.wrong_shift=wrong;fake.images[handle<VkImage>(11)].resize(160*32*4);fake.images[handle<VkImage>(12)].resize(160*32*4);
        float base[56]{};for(int i=0;i<3;++i){base[4*i+i]=1;base[44+4*i+i]=1;}base[28]=base[33]=1;base[38]=-(100.f+.05f)/(100.f-.05f);base[39]=-2.f*100.f*.05f/(100.f-.05f);base[42]=-1;for(int i=0;i<16;++i)base[12+i]=base[28+i];std::memcpy(fake.buffers.at(handle<VkBuffer>(10)).data(),base,sizeof base);
        if(in.view_color_probe){auto& bytes=fake.buffers.at(handle<VkBuffer>(10));
            for(auto field:std::initializer_list<std::pair<unsigned,float>>{{56,.05f},{57,100.f},{64,99.95f},{72,160.f},{73,32.f},{74,1.f/160},{75,1.f/32}})std::memcpy(bytes.data()+field.first*4,&field.second,4);}
        p=xr_baseline_recorder_create(in);check(p&&xr_baseline_recorder_prepare(p),"prepare baseline and shifted private images");large_draw(handle<VkImage>(11),0);large_draw(handle<VkImage>(12),0);
        check(xr_baseline_recorder_finish(p,1,9,3,in.command,in.writes)&&fake.draws==4&&fake.pushes==5&&fake.last_ubo==handle<VkBuffer>(10),"four private draws, complete source restore");
        check(!xr_baseline_recorder_retire(p,comparison),"all shifted storage waits for completion");fake.completed=9;check(xr_baseline_recorder_retire(p,comparison),"shifted resources retired");
        check(comparison.result==XrBaselineComparison::Result::Match&&comparison.depth_reference==depth&&comparison.view_shift_requested==translated&&comparison.matrix_probe_validated&&comparison.shifted_draw_requested&&comparison.shifted_draw_verified==!wrong,"correct shifted masks pass; wrong raster direction fails without weakening baseline");cleaned();
    }
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
