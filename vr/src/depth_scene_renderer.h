// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "depth_scene_spv.h"
#include "projection_math.h"
#include <filesystem>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <fstream>
#include <limits>
#include <cmath>
namespace hookshot::vr {
inline std::array<float,16> depth_scene_mvp(const XrPosef& eye,const XrPosef& anchor,const XrFovf& fov){
    for(const auto& pose:{eye,anchor}){
        const auto q=pose.orientation;const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
        if(!std::isfinite(norm)||std::abs(norm-1)>.01||!std::isfinite(pose.position.x)||!std::isfinite(pose.position.y)||!std::isfinite(pose.position.z))throw std::runtime_error("Invalid depth-scene tracking pose");
    }
    auto rotation=[](XrQuaternionf q){return std::array<double,9>{1-2*(q.y*q.y+q.z*q.z),2*(q.x*q.y-q.z*q.w),2*(q.x*q.z+q.y*q.w),2*(q.x*q.y+q.z*q.w),1-2*(q.x*q.x+q.z*q.z),2*(q.y*q.z-q.x*q.w),2*(q.x*q.z-q.y*q.w),2*(q.y*q.z+q.x*q.w),1-2*(q.x*q.x+q.y*q.y)};};
    const auto e=rotation(eye.orientation),a=rotation(anchor.orientation);double view[16]{};view[15]=1;
    const double delta[]={anchor.position.x-eye.position.x,anchor.position.y-eye.position.y,anchor.position.z-eye.position.z};
    for(int r=0;r<3;++r){for(int c=0;c<3;++c)for(int k=0;k<3;++k)view[4*r+c]+=e[3*k+r]*a[3*k+c];for(int k=0;k<3;++k)view[4*r+3]+=e[3*k+r]*delta[k];}
    ProjectionMatrix projection{};
    if(!eye_projection({fov.angleLeft,fov.angleRight,fov.angleUp,fov.angleDown},.025,150.,ClipDepth::ZeroToOne,DepthOrder::Forward,true,projection))throw std::runtime_error("Invalid XR depth-scene projection");
    std::array<float,16> out{};for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)out[4*r+c]+=float(projection[4*r+k]*view[4*k+c]);return out;
}
#define DEPTH_SCENE_FUNCTIONS(X) \
 X(CreateBuffer) X(DestroyBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) X(MapMemory) X(UnmapMemory) \
 X(CreateImage) X(DestroyImage) X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView) X(DestroyImageView) \
 X(CreateShaderModule) X(DestroyShaderModule) X(CreatePipelineLayout) X(DestroyPipelineLayout) X(CreateGraphicsPipelines) X(DestroyPipeline) \
 X(CmdBindPipeline) X(CmdBindVertexBuffers) X(CmdPushConstants) X(CmdDraw) X(CmdSetViewport) X(CmdSetScissor)
class DepthSceneRenderer {
    VkDevice device_=VK_NULL_HANDLE;PFN_vkGetPhysicalDeviceMemoryProperties properties_=nullptr;VkPhysicalDevice physical_=VK_NULL_HANDLE;
    VkBuffer vertices_=VK_NULL_HANDLE;VkDeviceMemory memory_=VK_NULL_HANDLE;VkPipelineLayout layout_=VK_NULL_HANDLE;VkPipeline pipeline_=VK_NULL_HANDLE;
    struct Depth {VkImage image=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;};std::array<Depth,2> depth_{};
    std::vector<float> data_;uint32_t count_=0;
#define DECL(n) PFN_vk##n n=nullptr;
    DEPTH_SCENE_FUNCTIONS(DECL)
#undef DECL
    static void checked(VkResult result,const char* what){if(result!=VK_SUCCESS)throw std::runtime_error(std::string(what)+": "+std::to_string(result));}
    uint32_t type(uint32_t bits,VkMemoryPropertyFlags flags){VkPhysicalDeviceMemoryProperties properties{};properties_(physical_,&properties);for(uint32_t i=0;i<properties.memoryTypeCount;++i)if((bits&(1u<<i))&&(properties.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No suitable frozen-scene memory type");}
public:
    bool enabled()const{return count_!=0;}
    uint32_t vertices()const{return count_;}
    void load(const std::string& path){
        std::ifstream file(std::filesystem::u8path(path),std::ios::binary|std::ios::ate);if(!file)throw std::runtime_error("Cannot open private depth mesh");const auto length=file.tellg();
        if(length<16||length>24000016)throw std::runtime_error("Depth mesh byte budget");file.seekg(0);
        char magic[8];float scale=0;uint32_t count=0;file.read(magic,8);file.read(reinterpret_cast<char*>(&count),4);file.read(reinterpret_cast<char*>(&scale),4);
        if(std::memcmp(magic,"HSVRM1\0\0",8)||count<3||count>1000000||count%3||length!=std::streamoff(16+uint64_t(count)*24)||!std::isfinite(scale)||scale<.0001f||scale>10)throw std::runtime_error("Invalid depth mesh header");
        std::vector<float> next(uint64_t(count)*6);file.read(reinterpret_cast<char*>(next.data()),std::streamsize(next.size()*4));if(!file)throw std::runtime_error("Truncated depth mesh");
        for(size_t i=0;i<next.size();++i)if(!std::isfinite(next[i])||std::abs(next[i])>(i%6<3?200.f:1.f)||(i%6>=3&&next[i]<0))throw std::runtime_error("Invalid depth mesh vertex/color");
        data_=std::move(next);count_=count;
    }
    void initialize(VkDevice device,VkPhysicalDevice physical,PFN_vkGetDeviceProcAddr get,PFN_vkGetPhysicalDeviceMemoryProperties properties,VkRenderPass pass){
        device_=device;physical_=physical;properties_=properties;
#define LOAD(n) n=reinterpret_cast<PFN_vk##n>(get(device,"vk" #n));if(!n)throw std::runtime_error("Missing vk" #n);
        DEPTH_SCENE_FUNCTIONS(LOAD)
#undef LOAD
        VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};buffer.size=data_.size()*4;buffer.usage=VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;buffer.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(CreateBuffer(device,&buffer,nullptr,&vertices_),"mesh buffer");VkMemoryRequirements req{};GetBufferMemoryRequirements(device,vertices_,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        checked(AllocateMemory(device,&alloc,nullptr,&memory_),"mesh memory");checked(BindBufferMemory(device,vertices_,memory_,0),"bind mesh memory");void* mapped=nullptr;
        checked(MapMemory(device,memory_,0,buffer.size,0,&mapped),"map mesh");std::memcpy(mapped,data_.data(),size_t(buffer.size));UnmapMemory(device,memory_);data_.clear();data_.shrink_to_fit();
        VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,64};VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};li.pushConstantRangeCount=1;li.pPushConstantRanges=&push;checked(CreatePipelineLayout(device,&li,nullptr,&layout_),"mesh pipeline layout");
        VkShaderModule modules[2]{};
        try{
            for(unsigned i=0;i<2;++i){VkShaderModuleCreateInfo si{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};si.pCode=i?depth_scene_frag_spv:depth_scene_vert_spv;si.codeSize=i?sizeof(depth_scene_frag_spv):sizeof(depth_scene_vert_spv);checked(CreateShaderModule(device,&si,nullptr,&modules[i]),"mesh shader");}
            VkPipelineShaderStageCreateInfo stages[2]{};for(unsigned i=0;i<2;++i){stages[i].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;stages[i].stage=i?VK_SHADER_STAGE_FRAGMENT_BIT:VK_SHADER_STAGE_VERTEX_BIT;stages[i].module=modules[i];stages[i].pName="main";}
            VkVertexInputBindingDescription binding{0,24,VK_VERTEX_INPUT_RATE_VERTEX};VkVertexInputAttributeDescription attrs[2]={{0,0,VK_FORMAT_R32G32B32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,12}};
            VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};vi.vertexBindingDescriptionCount=1;vi.pVertexBindingDescriptions=&binding;vi.vertexAttributeDescriptionCount=2;vi.pVertexAttributeDescriptions=attrs;
            VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};ia.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};viewport.viewportCount=viewport.scissorCount=1;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
            VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
            VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};ds.depthTestEnable=ds.depthWriteEnable=VK_TRUE;ds.depthCompareOp=VK_COMPARE_OP_LESS;
            VkPipelineColorBlendAttachmentState attachment{};attachment.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};blend.attachmentCount=1;blend.pAttachments=&attachment;
            VkDynamicState states[2]={VK_DYNAMIC_STATE_VIEWPORT,VK_DYNAMIC_STATE_SCISSOR};VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};dynamic.dynamicStateCount=2;dynamic.pDynamicStates=states;
            VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};info.stageCount=2;info.pStages=stages;info.pVertexInputState=&vi;info.pInputAssemblyState=&ia;info.pViewportState=&viewport;info.pRasterizationState=&raster;info.pMultisampleState=&ms;info.pDepthStencilState=&ds;info.pColorBlendState=&blend;info.pDynamicState=&dynamic;info.layout=layout_;info.renderPass=pass;
            checked(CreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&info,nullptr,&pipeline_),"mesh pipeline");
        }catch(...){for(auto module:modules)if(module)DestroyShaderModule(device,module,nullptr);throw;}
        for(auto module:modules)DestroyShaderModule(device,module,nullptr);
    }
    VkImageView depth(unsigned eye,uint32_t width,uint32_t height){auto& d=depth_.at(eye);
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=VK_FORMAT_D32_SFLOAT;info.extent={width,height,1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(CreateImage(device_,&info,nullptr,&d.image),"scene depth image");VkMemoryRequirements req{};GetImageMemoryRequirements(device_,d.image,&req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};alloc.allocationSize=req.size;alloc.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);checked(AllocateMemory(device_,&alloc,nullptr,&d.memory),"scene depth memory");checked(BindImageMemory(device_,d.image,d.memory,0),"bind scene depth");
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=d.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=VK_FORMAT_D32_SFLOAT;view.subresourceRange={VK_IMAGE_ASPECT_DEPTH_BIT,0,1,0,1};checked(CreateImageView(device_,&view,nullptr,&d.view),"scene depth view");return d.view;
    }
    void draw(VkCommandBuffer cmd,uint32_t width,uint32_t height,const std::array<float,16>& mvp){
        VkViewport viewport{0,0,float(width),float(height),0,1};VkRect2D scissor{{0,0},{width,height}};VkDeviceSize offset=0;
        CmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_);CmdSetViewport(cmd,0,1,&viewport);CmdSetScissor(cmd,0,1,&scissor);CmdBindVertexBuffers(cmd,0,1,&vertices_,&offset);CmdPushConstants(cmd,layout_,VK_SHADER_STAGE_VERTEX_BIT,0,64,mvp.data());CmdDraw(cmd,count_,1,0,0);
    }
    void close(){if(!device_)return;
        for(auto& d:depth_){if(d.view&&DestroyImageView)DestroyImageView(device_,d.view,nullptr);if(d.image&&DestroyImage)DestroyImage(device_,d.image,nullptr);if(d.memory&&FreeMemory)FreeMemory(device_,d.memory,nullptr);d={};}
        if(pipeline_&&DestroyPipeline)DestroyPipeline(device_,pipeline_,nullptr);if(layout_&&DestroyPipelineLayout)DestroyPipelineLayout(device_,layout_,nullptr);if(vertices_&&DestroyBuffer)DestroyBuffer(device_,vertices_,nullptr);if(memory_&&FreeMemory)FreeMemory(device_,memory_,nullptr);pipeline_=VK_NULL_HANDLE;layout_=VK_NULL_HANDLE;vertices_=VK_NULL_HANDLE;memory_=VK_NULL_HANDLE;device_=VK_NULL_HANDLE;
    }
};
#undef DEPTH_SCENE_FUNCTIONS
}
