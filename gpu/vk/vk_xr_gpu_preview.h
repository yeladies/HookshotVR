// SPDX-License-Identifier: GPL-3.0-or-later
// GPU-only world preview. The source owner supplies one immutable completed pair.
#pragma once
#include "xr_gpu_eye_stream.h"
#include "xr_same_state_buffer_copy.h"
#include "xr_gpu_projection_layer.h"
#include <filesystem>
#include <fstream>
namespace mkwvk { namespace {
#define GPU_PREVIEW_VK_FUNCTIONS(X) \
 X(GetDeviceQueue) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) X(ResetCommandBuffer) \
 X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(ResetFences) X(WaitForFences) \
 X(QueueSubmit) X(CmdPipelineBarrier) X(CmdCopyBufferToImage)
struct GpuPairPreview {
    Bridge& bridge;std::mutex& queue_mutex;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;uint32_t family=0,width=0,height=0,index=0;
    XrSession session=XR_NULL_HANDLE;XrSwapchain chain=XR_NULL_HANDLE;std::vector<XrSwapchainImageVulkan2KHR> images;
    VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
    bool acquired=false,waited=false,pending=false,uncertain=false,released_valid=false,layer_ready=false;
    uint64_t generation=0,copy_sequence=0,last_state=0,layer_frames=0,image_updates=0,submitted_frames=0;
    std::shared_ptr<const XrGpuEyeFrame> held;
    hookshot::vr::GpuProjectionPair last_metadata;hookshot::vr::GpuProjectionData projection_data;XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    PFN_xrDestroySwapchain destroy_chain=nullptr;PFN_xrAcquireSwapchainImage acquire=nullptr;PFN_xrWaitSwapchainImage wait_image=nullptr;PFN_xrReleaseSwapchainImage release=nullptr;
#define DECL(n) PFN_vk##n n=nullptr;
    GPU_PREVIEW_VK_FUNCTIONS(DECL)
#undef DECL
    GpuPairPreview(Bridge& owner,std::mutex& mutex):bridge(owner),queue_mutex(mutex){}
    static int64_t now_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
    void report(const char* status)const{
        const char* path=std::getenv("HOOKSHOT_VR_GPU_WORLD_REPORT_FILE");if(!path||!*path)return;
        try{std::ofstream f(std::filesystem::u8path(path),std::ios::trunc);if(f)f<<"{\"schema\":1,\"status\":\""<<status<<"\",\"width_per_eye\":"<<width<<",\"height\":"<<height<<",\"device_generation\":"<<generation<<",\"source_state\":"<<last_metadata.source_state<<",\"origin\":"<<last_metadata.target.origin<<",\"gpu_pair_copies\":"<<image_updates<<",\"projection_frames_submitted\":"<<submitted_frames<<",\"CPU_pixel_staging\":false,\"production_pose_metadata\":true,\"same_source_state\":true,\"native_geometry_stereo\":false,\"pending\":"<<(pending?"true":"false")<<",\"acquired\":"<<(acquired?"true":"false")<<",\"uncertain\":"<<(uncertain?"true":"false")<<"}\n";}catch(...){}
    }
    void submitted(){++submitted_frames;if(submitted_frames==1||submitted_frames%60==0)report("presenting");}
    bool vk_ok(VkResult r,const char* label){if(r==VK_SUCCESS)return true;logf("xr-gpu: %s failed (%d)",label,int(r));return false;}
    static uint32_t extent(const char* name,uint32_t maximum){const char* value=std::getenv(name);if(!value||!*value)return 1024;char* end=nullptr;const auto n=std::strtoul(value,&end,10);return end==value||*end||n<8||n>maximum||n%8?0:uint32_t(n);}
    bool initialize(XrSession s,VkInstance instance,VkPhysicalDevice,VkDevice d,uint32_t queue_family){
        session=s;device=d;family=queue_family;width=extent("HOOKSHOT_VR_GPU_EYE_WIDTH",1920);height=extent("HOOKSHOT_VR_GPU_EYE_HEIGHT",1080);if(!width||!height||!bridge.vulkan_get)return false;
        auto get_device=reinterpret_cast<PFN_vkGetDeviceProcAddr>(bridge.vulkan_get(instance,"vkGetDeviceProcAddr"));if(!get_device)return false;
#define LOAD(n) n=reinterpret_cast<PFN_vk##n>(get_device(device,"vk" #n));if(!n){logf("xr-gpu: missing vk%s",#n);return false;}
        GPU_PREVIEW_VK_FUNCTIONS(LOAD)
#undef LOAD
        PFN_xrEnumerateSwapchainFormats formats=nullptr;PFN_xrCreateSwapchain create=nullptr;PFN_xrEnumerateSwapchainImages enumerate=nullptr;
        if(!bridge.function("xrEnumerateSwapchainFormats",formats)||!bridge.function("xrCreateSwapchain",create)||!bridge.function("xrDestroySwapchain",destroy_chain)||
           !bridge.function("xrEnumerateSwapchainImages",enumerate)||!bridge.function("xrAcquireSwapchainImage",acquire)||!bridge.function("xrWaitSwapchainImage",wait_image)||!bridge.function("xrReleaseSwapchainImage",release))return false;
        uint32_t count=0;if(!ok(formats(session,0,&count,nullptr),"GPU format count")||!count||count>256)return false;std::vector<int64_t> values(count);if(!ok(formats(session,count,&count,values.data()),"GPU formats"))return false;
        if(std::find(values.begin(),values.end(),int64_t(VK_FORMAT_R8G8B8A8_UNORM))==values.end()){logf("xr-gpu: RGBA8 UNORM unavailable; no unverified color conversion fallback");return false;}
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;ci.format=VK_FORMAT_R8G8B8A8_UNORM;ci.sampleCount=1;ci.width=2*width;ci.height=height;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
        if(!ok(create(session,&ci,&chain),"create GPU pair swapchain")||!ok(enumerate(chain,0,&count,nullptr),"GPU image count")||!count||count>16)return false;images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        if(!ok(enumerate(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"GPU images"))return false;
        GetDeviceQueue(device,family,0,&queue);VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=family;
        if(!vk_ok(CreateCommandPool(device,&cp,nullptr,&pool),"create copy pool"))return false;VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
        if(!vk_ok(AllocateCommandBuffers(device,&ca,&command),"allocate copy command"))return false;VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};if(!vk_ok(CreateFence(device,&fi,nullptr,&fence),"create copy fence"))return false;
        logf("xr-gpu: pair swapchain ready, %ux%u per eye, no CPU pixel staging",width,height);report("initialized");return true;
    }
    static hookshot::vr::GpuProjectionPair metadata(const XrGpuEyeFrame& f){hookshot::vr::GpuProjectionPair p;p.source_state=f.source_state;p.device_generation=f.generation;p.width=f.width;p.height=f.height;p.source_observed_ns=f.source_observed_ns;p.ready_ns=f.ready_ns;p.immutable=p.complete=true;p.target=f.target;return p;}
    bool valid(const XrGpuEyeFrame& f,uint64_t origin,int64_t now){return f.device==device&&f.buffer&&f.bytes==uint64_t(width)*height*8&&f.state&&f.recording&&f.width==width&&f.height==height&&f.queue_family==family&&f.format==VK_FORMAT_R8G8B8A8_UNORM&&!f.synthetic&&hookshot::vr::gpu_projection_pair_valid(metadata(f),origin,now);}
    bool show_previous(XrSpace local,uint64_t origin,int64_t now){
        layer_ready=false;if(!released_valid)return true;const hookshot::vr::GpuProjectionSwapchain image{chain,generation,2*width,height,1,true,true};
        if(hookshot::vr::prepare_gpu_projection_layer(last_metadata,image,local,origin,now,projection_data)){projection=projection_data.layer();layer_ready=true;++layer_frames;}return true;
    }
    bool release_image(){if(!acquired)return true;if(!waited||pending||uncertain)return false;XrSwapchainImageReleaseInfo info{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};std::lock_guard lock(queue_mutex);if(!ok(release(chain,&info),"release GPU pair image")){uncertain=true;return false;}acquired=waited=false;return true;}
    bool finish(XrSpace local,uint64_t origin,uint64_t timeout){
        const auto result=WaitForFences(device,1,&fence,VK_TRUE,timeout);if(result==VK_TIMEOUT)return show_previous(local,origin,now_ns());if(!vk_ok(result,"complete pair copy")){uncertain=true;return false;}
        pending=false;if(!release_image())return false;last_metadata=metadata(*held);last_state=held->state;released_valid=true;++image_updates;held.reset();return show_previous(local,origin,now_ns());
    }
    bool frame(XrSpace local,uint64_t origin){
        layer_ready=false;if(uncertain)return false;if(pending)return finish(local,origin,0);
        if(!acquired){auto next=xr_gpu_eye_stream_acquire(device,origin,now_ns());if(!next||!valid(*next,origin,now_ns()))return show_previous(local,origin,now_ns());
            if(generation&&generation!=next->generation){uncertain=true;return false;}if(!generation)generation=next->generation;
            if(released_valid&&next->state==last_state)return show_previous(local,origin,now_ns());held=std::move(next);XrSwapchainImageAcquireInfo info{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            {std::lock_guard lock(queue_mutex);if(!ok(acquire(chain,&info,&index),"acquire GPU pair image")){held.reset();return false;}}acquired=true;waited=false;
        }
        if(!waited){XrSwapchainImageWaitInfo info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};info.timeout=0;const auto result=wait_image(chain,&info);if(result==XR_TIMEOUT_EXPIRED)return show_previous(local,origin,now_ns());if(!ok(result,"wait GPU pair image"))return false;waited=true;}
        if(index>=images.size()||!held||!valid(*held,origin,now_ns())){released_valid=false;if(!release_image())return false;held.reset();return index<images.size();}
        if(!vk_ok(ResetCommandBuffer(command,0),"reset pair copy"))return false;VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;if(!vk_ok(BeginCommandBuffer(command,&begin),"begin pair copy"))return false;
        const uint64_t recording=++copy_sequence;const hookshot::vr::SameStateBufferPair source{device,held->buffer,held->bytes,held->state,origin,held->target.serial,recording,generation,width,height,family,held->format};
        const hookshot::vr::SameStateBufferTarget target{device,images[index].image,generation,2*width,height,family,held->format,true,true};
        if(!hookshot::vr::record_same_state_buffer_copy(command,recording,source,target,CmdPipelineBarrier,CmdCopyBufferToImage)||!vk_ok(EndCommandBuffer(command),"end pair copy")||!vk_ok(ResetFences(device,1,&fence),"reset pair fence"))return false;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;pending=true;
        {std::lock_guard lock(queue_mutex);if(!vk_ok(QueueSubmit(queue,1,&submit,fence),"submit pair copy")){uncertain=true;return false;}}
        return finish(local,origin,1000000);
    }
    const XrCompositionLayerBaseHeader* submitted_layer()const{return layer_ready?reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection):nullptr;}
    bool close(){
        if(uncertain)return false;if(pending){const auto result=WaitForFences(device,1,&fence,VK_TRUE,250000000);if(!vk_ok(result,"drain GPU pair copy"))return false;pending=false;}
        if(acquired&&!waited){XrSwapchainImageWaitInfo info{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};info.timeout=250000000;if(wait_image(chain,&info)!=XR_SUCCESS)return false;waited=true;}
        if(!release_image())return false;held.reset();if(fence&&DestroyFence)DestroyFence(device,fence,nullptr);fence=VK_NULL_HANDLE;if(pool&&DestroyCommandPool)DestroyCommandPool(device,pool,nullptr);pool=VK_NULL_HANDLE;
        if(chain&&destroy_chain&&!ok(destroy_chain(chain),"destroy GPU pair swapchain"))return false;chain=XR_NULL_HANDLE;
        logf("xr-gpu: closed; projection frames %llu, GPU pair copies %llu",(unsigned long long)submitted_frames,(unsigned long long)image_updates);report("closed");return true;
    }
};
#undef GPU_PREVIEW_VK_FUNCTIONS
} }
