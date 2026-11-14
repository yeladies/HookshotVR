// SPDX-License-Identifier: GPL-3.0-or-later
// Authored inputs only. No game, OpenXR session, window or display mutation.
#define VK_NO_PROTOTYPES
#include "gpu_smoke_context.h"
#include "depth_pair_reproject.h"
#include "depth_pair_reproject_spv.h"
#include "same_state_gpu_copy.h"
#include <iostream>
#include <algorithm>
using namespace hookshot::vr;
using namespace hookshot::vr::smoke;
namespace {
uint32_t float_word(float f){uint32_t v;std::memcpy(&v,&f,4);return v;}
uint32_t mini_float(float f,unsigned mantissa){if(!f)return 0;const auto bits=float_word(f);return (((bits>>23)&255)-112)<<mantissa|((bits&0x7fffff)>>(23-mantissa));}
uint32_t expected_color(unsigned x,unsigned y,bool flip){if(flip)x=79-x;auto byte=[](unsigned v){return uint32_t(std::floor(double(v)*255/128+.5));};return byte(x)|(byte(y)<<8)|(64u<<16)|0xff000000u;}
DepthPairParameters parameters(unsigned eye,unsigned round){DepthPairParameters p;const float angle=round==2?.07f:0,co=std::cos(angle),si=std::sin(angle);
    p.source_from_eye={co,0,si,eye?.0625f:-.0625f,0,1,0,0,-si,0,co,round==2?.05f:0};
    p.eye_tangents={-1,1,1,-1};p.projection={.8f,.8f,float(-10.1/9.9),float(-2./9.9)};p.optical={0,0,100,1.25f};p.dimensions={80,80,64,64};p.settings={round==1?122u:123u,4,0,0};return p;
}
std::array<unsigned,2> analytic_source_pixel(const DepthPairParameters& p,unsigned x,unsigned y){
    const double a=2*(x+.5)/64-1,b=1-2*(y+.5)/64;const auto& r=p.source_from_eye;
    const double dx=r[0]*a+r[1]*b-r[2],dy=r[4]*a+r[5]*b-r[6],dz=r[8]*a+r[9]*b-r[10];
    const double t=(-2.-r[11])/dz;const double sx=r[3]+dx*t,sy=r[7]+dy*t;
    const int px=int(std::floor((double(p.projection[0])*sx/2+1)*40));const int py=int(std::floor((1-double(p.projection[1])*sy/2)*40));
    require(px>=0&&px<80&&py>=0&&py<80,"analytic plane remains within overscan");return {unsigned(px),unsigned(py)};
}
}
int main(int argc,char** argv){if(argc!=2||std::strcmp(argv[1],"--run")){std::cerr<<"Explicit native GPU opt-in: --run\n";return 2;}
try{
    Context c;const auto gpu=initialize_smoke(c);const auto api=gpu.api;
    auto retire=[&c](EyeResourcePair* p){if(!c.uncertain)delete p;};
    std::unique_ptr<EyeResourcePair,decltype(retire)> eyes(new EyeResourcePair,retire),target(new EyeResourcePair,retire);
    require(eyes->create(c.device,api,gpu.memory,64,64,VK_FORMAT_R8G8B8A8_SRGB,16,1024*1024),"allocate independent eye images");
    require(target->create(c.device,api,gpu.memory,128,64,VK_FORMAT_R8G8B8A8_SRGB,16,1024*1024),"allocate same-state pair target");
    address_buffer(c,api,gpu.memory,51200,c.auxiliary[0]);address_buffer(c,api,gpu.memory,256,c.auxiliary[1]);address_buffer(c,api,gpu.memory,32768,c.auxiliary[2]);address_buffer(c,api,gpu.memory,32,c.auxiliary[3]);
    VkBufferCreateInfo staging{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};staging.size=84000;staging.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    checked(api.create_buffer(c.device,&staging,nullptr,&c.staging),"create bounded readback");VkMemoryRequirements requirements{};api.buffer_requirements(c.device,c.staging,&requirements);require(requirements.size<=1024*1024,"readback allocation bound");
    uint32_t type=UINT32_MAX;bool coherent=false;for(uint32_t i=0;i<gpu.memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){type=i;coherent=(gpu.memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;break;}
    require(type!=UINT32_MAX,"readback host visibility");VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;checked(api.allocate(c.device,&allocation,nullptr,&c.memory),"allocate readback");checked(api.bind_buffer(c.device,c.staging,c.memory,0),"bind readback");
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader.codeSize=sizeof kDepthPairReprojectSpv;shader.pCode=kDepthPairReprojectSpv;
    checked(c.fn<PFN_vkCreateShaderModule>("vkCreateShaderModule")(c.device,&shader,nullptr,&c.shader),"create authored depth shader");VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(DepthPairDispatch)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&range;checked(c.fn<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(c.device,&layout,nullptr,&c.layout),"create depth layout");
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline.layout=c.layout;pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pipeline.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline.stage.module=c.shader;pipeline.stage.pName="main";
    checked(c.fn<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(c.device,VK_NULL_HANDLE,1,&pipeline,nullptr,&c.pipeline),"create depth pipeline");
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=gpu.family;checked(c.fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(c.device,&pool,nullptr,&c.pool),"create compute pool");
    VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commands.commandPool=c.pool;commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=1;VkCommandBuffer command=VK_NULL_HANDLE;checked(c.fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(c.device,&commands,&command),"allocate command");
    const auto barrier=c.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");auto order=[&](VkPipelineStageFlags source,VkPipelineStageFlags destination,VkAccessFlags read,VkAccessFlags write){VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=read;b.dstAccessMask=write;barrier(command,source,destination,0,1,&b,0,nullptr,0,nullptr);};
    for(unsigned round=0;round<4;++round){
        std::vector<uint32_t> source(12800);const bool flip=round==1;
        const float depth=float((1.-double(parameters(0,round).projection[2])+double(parameters(0,round).projection[3])/2)/2);
        for(unsigned y=0;y<80;++y)for(unsigned x=0;x<80;++x){const unsigned sx=flip?79-x:x,index=y*80+x;
            source[index]=round==1?mini_float(float(sx)/128,6)|(mini_float(float(y)/128,6)<<11)|(mini_float(.25f,5)<<22):sx*4|(y*4<<9)|(128u<<18)|(15u<<27);
            source[6400+index]=round==3?0x7fc00001u:float_word(depth);
        }
        const std::array<DepthPairParameters,2> cameras{parameters(0,round),parameters(1,round)};
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(c.fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(command,&begin),"begin GPU pair");
        c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[0].buffer,0,51200,source.data());
        c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[1].buffer,0,sizeof cameras,cameras.data());
        c.fn<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(command,c.auxiliary[3].buffer,0,32,0);
        order(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT);
        c.fn<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(command,VK_PIPELINE_BIND_POINT_COMPUTE,c.pipeline);
        DepthPairSource identity{c.auxiliary[0].address,51200,round+1,1,round+1,round+1,1,80,80,cameras[0].settings[0],gpu.family,true,true};
        identity.projection=cameras[0].projection;identity.optical_center={cameras[0].optical[0],cameras[0].optical[1]};
        std::array<DepthPairOutput,2> destinations;for(unsigned eye=0;eye<2;++eye)destinations[eye]={c.auxiliary[2].address+eye*16384,16384,c.auxiliary[1].address+eye*128,128,c.auxiliary[3].address+eye*16,16,round+1,1,round+1,1,eye,gpu.family};
        std::array<DepthPairDispatch,2> dispatches;require(prepare_depth_pair_dispatches(identity,round+1,1,destinations,cameras,dispatches),"admit both eyes from one source state and pose scope");
        for(unsigned eye=0;eye<2;++eye){const auto& dispatch=dispatches[eye];
            require(depth_pair_dispatch_valid(dispatch,cameras[eye],51200,16384,128,16),"admit bounded source/output dispatch");
            c.fn<PFN_vkCmdPushConstants>("vkCmdPushConstants")(command,c.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof dispatch,&dispatch);c.fn<PFN_vkCmdDispatch>("vkCmdDispatch")(command,8,8,1);
        }
        order(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
        std::array<VkImageMemoryBarrier,2> image_barriers{};
        for(unsigned eye=0;eye<2;++eye){auto& b=image_barriers[eye];b.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};b.image=eyes->eye(eye)->image;b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;}
        barrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,image_barriers.data());
        for(unsigned eye=0;eye<2;++eye){VkBufferImageCopy copy{};copy.bufferOffset=eye*16384;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={64,64,1};c.fn<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(command,c.auxiliary[2].buffer,eyes->eye(eye)->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            image_barriers[eye].oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image_barriers[eye].newLayout=VK_IMAGE_LAYOUT_GENERAL;image_barriers[eye].srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image_barriers[eye].dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;}
        barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,2,image_barriers.data());
        std::array<SameStateEyeImage,2> pair;for(unsigned eye=0;eye<2;++eye)pair[eye]={eyes->eye(eye)->image,round+1,1,round+1,round+1,eye,64,64,gpu.family,VK_FORMAT_R8G8B8A8_SRGB};
        require(record_same_state_eye_copy(command,round+1,pair,{target->eye(0)->image,128,64,gpu.family,VK_FORMAT_R8G8B8A8_SRGB,true,true},barrier,c.fn<PFN_vkCmdCopyImage>("vkCmdCopyImage")),"copy complete same-source eye pair without intermediate CPU pixels");
        VkImageMemoryBarrier read{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};read.srcQueueFamilyIndex=read.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;read.image=target->eye(0)->image;read.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};read.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;read.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;read.srcAccessMask=VK_ACCESS_MEMORY_WRITE_BIT;read.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        barrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&read);
        VkBufferImageCopy image_copy{};image_copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};image_copy.imageExtent={128,64,1};c.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,target->eye(0)->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.staging,1,&image_copy);
        VkBufferCopy source_copy{0,32768,51200},counter_copy{0,83968,32};c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,c.auxiliary[0].buffer,c.staging,1,&source_copy);c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,c.auxiliary[3].buffer,c.staging,1,&counter_copy);
        order(VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT);checked(c.fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command),"end GPU pair");
        const auto submitted=eyes->submit(c.queue,command);c.uncertain=eyes->pending();checked(submitted,"submit same-source pair");require(eyes->poll(5000000000ull),"GPU pair timeout; allocations retained");c.uncertain=false;
        void* mapped=nullptr;checked(c.fn<PFN_vkMapMemory>("vkMapMemory")(c.device,c.memory,0,VK_WHOLE_SIZE,0,&mapped),"map completed output");if(!coherent){VkMappedMemoryRange invalidate{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};invalidate.memory=c.memory;invalidate.offset=0;invalidate.size=VK_WHOLE_SIZE;checked(c.fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(c.device,1,&invalidate),"invalidate completed output");}
        const auto* bytes=static_cast<const unsigned char*>(mapped);require(!std::memcmp(bytes+32768,source.data(),51200),"original GPU color/depth packet unchanged");
        for(unsigned eye=0;eye<2;++eye){uint32_t counters[4]{};std::memcpy(counters,bytes+83968+eye*16,16);
            require(counters[0]==(round==3?0u:4096u)&&counters[1]==0&&counters[2]==(round==3?4096u:0u)&&counters[3]==0,"complete plane coverage or complete invalid-depth rejection");
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){uint32_t actual=0;std::memcpy(&actual,bytes+(y*128+eye*64+x)*4,4);uint32_t expected=0xff000000u;
                if(round!=3){const auto coordinate=analytic_source_pixel(cameras[eye],x,y);expected=expected_color(coordinate[0],coordinate[1],flip);}
                for(unsigned channel=0;channel<4;++channel){const int error=int((actual>>(channel*8))&255)-int((expected>>(channel*8))&255);require(std::abs(error)<=(round==2&&channel<2?2:0),"GPU stereo pixel disagrees with independent ray/plane oracle");}
            }
        }
        c.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(c.device,c.memory);
        if(round<3){require(eyes->rearm(),"rearm only completed allocations");checked(c.fn<PFN_vkResetCommandPool>("vkResetCommandPool")(c.device,c.pool,0),"reuse completed command storage");}
    }
    std::cout<<"same_source_depth_pair=true; GPU="<<gpu.props.deviceName<<"; target_pixels_checked=32768; source_formats_checked=2; rotated_plane_verified=true; invalid_depth_rejected=true; original_color_depth_unchanged=true; intermediate_CPU_pixels=false; submissions=4; allocations_reused=true; XR=false; game=false\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
