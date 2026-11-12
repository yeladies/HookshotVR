// SPDX-License-Identifier: GPL-3.0-or-later
// Flat game image on a LOCAL-space quad. This is transport validation, not native game stereo.
#pragma once
#include <cmath>
namespace mkwvk { namespace {
#define PREVIEW_VK_FUNCTIONS(X) \
 X(GetDeviceQueue) X(CreateBuffer) X(GetBufferMemoryRequirements) X(AllocateMemory) X(BindBufferMemory) X(MapMemory) \
 X(UnmapMemory) X(FreeMemory) X(DestroyBuffer) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) \
 X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) X(ResetFences) \
 X(WaitForFences) X(QueueSubmit) X(CmdPipelineBarrier) X(CmdCopyBufferToImage) X(FlushMappedMemoryRanges)
struct QuadPreview {
    Bridge& bridge;std::mutex& queue_mutex;
    XrSession session=XR_NULL_HANDLE;XrSwapchain chain=XR_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;
    VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void* mapped=nullptr;
    VkCommandPool pool=VK_NULL_HANDLE;VkCommandBuffer command=VK_NULL_HANDLE;VkFence fence=VK_NULL_HANDLE;
    bool coherent=false,pending=false,anchored=false;
    uint64_t anchor_generation=0,quad_frames=0,game_serial=0,game_updates=0,staged_serial=0;
    std::vector<XrSwapchainImageVulkan2KHR> images;
    XrPosef anchor{};
    XrCompositionLayerQuad layer{XR_TYPE_COMPOSITION_LAYER_QUAD};
    PFN_xrDestroySwapchain destroy_chain=nullptr;
    PFN_xrAcquireSwapchainImage acquire=nullptr;PFN_xrWaitSwapchainImage wait_image=nullptr;PFN_xrReleaseSwapchainImage release=nullptr;
#define DECL(n) PFN_vk##n n=nullptr;
    PREVIEW_VK_FUNCTIONS(DECL)
#undef DECL
    QuadPreview(Bridge& owner,std::mutex& mutex):bridge(owner),queue_mutex(mutex) {}
    bool vk_ok(VkResult result,const char* operation) {if(result==VK_SUCCESS)return true;logf("xr: preview %s failed (%d)",operation,int(result));return false;}
    bool initialize(XrSession s,VkInstance instance,VkPhysicalDevice physical,VkDevice d,uint32_t family) {
        session=s;device=d;
        if(!bridge.vulkan_get)return false;
        auto get_device=reinterpret_cast<PFN_vkGetDeviceProcAddr>(bridge.vulkan_get(instance,"vkGetDeviceProcAddr"));
        auto memory_properties=reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(bridge.vulkan_get(instance,"vkGetPhysicalDeviceMemoryProperties"));
        if(!get_device||!memory_properties)return false;
#define LOAD(n) n=reinterpret_cast<PFN_vk##n>(get_device(device,"vk" #n));if(!n){logf("xr: preview missing vk%s",#n);return false;}
        PREVIEW_VK_FUNCTIONS(LOAD)
#undef LOAD
        PFN_xrEnumerateSwapchainFormats formats=nullptr;PFN_xrCreateSwapchain create_chain=nullptr;PFN_xrEnumerateSwapchainImages enumerate=nullptr;
        if(!bridge.function("xrEnumerateSwapchainFormats",formats)||!bridge.function("xrCreateSwapchain",create_chain)||
           !bridge.function("xrDestroySwapchain",destroy_chain)||!bridge.function("xrEnumerateSwapchainImages",enumerate)||
           !bridge.function("xrAcquireSwapchainImage",acquire)||!bridge.function("xrWaitSwapchainImage",wait_image)||!bridge.function("xrReleaseSwapchainImage",release))return false;
        uint32_t count=0;if(!ok(formats(session,0,&count,nullptr),"preview format count"))return false;
        std::vector<int64_t> supported(count);if(!ok(formats(session,count,&count,supported.data()),"preview formats"))return false;
        if(std::find(supported.begin(),supported.end(),int64_t(VK_FORMAT_R8G8B8A8_SRGB))==supported.end()) {logf("xr: preview sRGB format unavailable");return false;}
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        ci.format=VK_FORMAT_R8G8B8A8_SRGB;ci.sampleCount=1;ci.width=1280;ci.height=720;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
        if(!ok(create_chain(session,&ci,&chain),"create preview swapchain")||!ok(enumerate(chain,0,&count,nullptr),"preview image count")||!count)return false;
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
        if(!ok(enumerate(chain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())),"preview images"))return false;
        GetDeviceQueue(device,family,0,&queue);
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};b.size=1280*720*4;b.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;b.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        if(!vk_ok(CreateBuffer(device,&b,nullptr,&buffer),"create staging buffer"))return false;
        VkMemoryRequirements requirements{};GetBufferMemoryRequirements(device,buffer,&requirements);
        VkPhysicalDeviceMemoryProperties properties{};memory_properties(physical,&properties);
        uint32_t type=UINT32_MAX;
        for(uint32_t i=0;i<properties.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&(properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            if(type==UINT32_MAX)type=i;
            if(properties.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT){type=i;break;}
        }
        if(type==UINT32_MAX)return false;
        coherent=(properties.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
        if(!vk_ok(AllocateMemory(device,&allocation,nullptr,&memory),"allocate staging memory")||!vk_ok(BindBufferMemory(device,buffer,memory,0),"bind staging buffer")||
           !vk_ok(MapMemory(device,memory,0,VK_WHOLE_SIZE,0,&mapped),"map staging memory"))return false;
        VkCommandPoolCreateInfo p{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};p.queueFamilyIndex=family;p.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if(!vk_ok(CreateCommandPool(device,&p,nullptr,&pool),"create command pool"))return false;
        VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};commands.commandPool=pool;commands.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;commands.commandBufferCount=1;
        if(!vk_ok(AllocateCommandBuffers(device,&commands,&command),"allocate command buffer"))return false;
        VkFenceCreateInfo f{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};f.flags=VK_FENCE_CREATE_SIGNALED_BIT;
        if(!vk_ok(CreateFence(device,&f,nullptr,&fence),"create upload fence"))return false;
        logf("xr: flat quad preview prepared, 1280x720 sRGB; not native game stereo");return true;
    }
    bool upload(uint32_t index,const PreviewFrame& frame) {
        if(pending)return false;
        if(staged_serial!=frame.serial) {
            std::memcpy(mapped,frame.pixels->data(),frame.pixels->size());
            if(!coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=memory;range.size=VK_WHOLE_SIZE;
                if(!vk_ok(FlushMappedMemoryRanges(device,1,&range),"flush staging memory"))return false;}
            staged_serial=frame.serial;
        }
        if(!vk_ok(ResetCommandBuffer(command,0),"reset upload command"))return false;
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if(!vk_ok(BeginCommandBuffer(command,&begin),"begin upload command"))return false;
        VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
        host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=buffer;host.size=VK_WHOLE_SIZE;
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,1,&host,0,nullptr);
        VkImageMemoryBarrier image{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};image.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
        image.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;image.image=images[index].image;image.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&image);
        VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={1280,720,1};
        CmdCopyBufferToImage(command,buffer,images[index].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
        image.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;image.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT;
        image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;image.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        CmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,0,0,nullptr,0,nullptr,1,&image);
        if(!vk_ok(EndCommandBuffer(command),"end upload command")||!vk_ok(ResetFences(device,1,&fence),"reset upload fence"))return false;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        {std::lock_guard lock(queue_mutex);if(!vk_ok(QueueSubmit(queue,1,&submit,fence),"submit image upload"))return false;}
        pending=true;
        if(!vk_ok(WaitForFences(device,1,&fence,VK_TRUE,5000000000ull),"wait image upload"))return false;
        pending=false;return true;
    }
    bool frame(XrSpace local,const XrPosef& head,uint64_t generation,PreviewFrame frame) {
        if(!frame.pixels||frame.width!=1280||frame.height!=720){logf("xr: preview frame-size validation failed");return false;}
        const auto q0=head.orientation;const double norm=double(q0.x)*q0.x+double(q0.y)*q0.y+double(q0.z)*q0.z+double(q0.w)*q0.w;
        if(!std::isfinite(norm)||std::abs(norm-1)>0.01||!std::isfinite(head.position.x)||!std::isfinite(head.position.y)||!std::isfinite(head.position.z)){logf("xr: preview pose validation failed");return false;}
        if(!anchored||anchor_generation!=generation) {
            const auto q=head.orientation;
            const float yaw=std::atan2(2*(q.x*q.z+q.w*q.y),1-2*(q.x*q.x+q.y*q.y));
            anchor.orientation={0,std::sin(yaw/2),0,std::cos(yaw/2)};
            anchor.position={head.position.x-2*std::sin(yaw),head.position.y,head.position.z-2*std::cos(yaw)};
            anchored=true;anchor_generation=generation;
        }
        // xrEndFrame uses the most recently released image. Reuse it until a new game image arrives.
        if(game_serial && game_serial==frame.serial) {layer.space=local;layer.pose=anchor;++quad_frames;return true;}
        uint32_t index=0;XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        {std::lock_guard lock(queue_mutex);if(!ok(acquire(chain,&a,&index),"acquire preview image"))return false;}
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};wait.timeout=1000000000;
        const auto ready=wait_image(chain,&wait);
        if(ready!=XR_SUCCESS){logf("xr: preview image wait failed (%d)",int(ready));return false;}
        if(index>=images.size()){logf("xr: preview image-index validation failed");return false;}
        const bool uploaded=upload(index,frame);
        if(!pending){XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};std::lock_guard lock(queue_mutex);
            if(!ok(release(chain,&r),"release preview image"))return false;}
        if(!uploaded)return false;
        layer.space=local;layer.eyeVisibility=XR_EYE_VISIBILITY_BOTH;layer.pose=anchor;layer.size={2.4f,1.35f};
        layer.subImage={chain,{{0,0},{1280,720}},0};if(game_serial!=frame.serial)++game_updates;game_serial=frame.serial;++quad_frames;
        return true;
    }
    bool close() {
        if(pending) {
            if(!vk_ok(WaitForFences(device,1,&fence,VK_TRUE,5000000000ull),"drain image upload"))return false;
            pending=false;
        }
        if(fence&&DestroyFence)DestroyFence(device,fence,nullptr);fence=VK_NULL_HANDLE;
        if(pool&&DestroyCommandPool)DestroyCommandPool(device,pool,nullptr);pool=VK_NULL_HANDLE;
        if(mapped&&UnmapMemory)UnmapMemory(device,memory);mapped=nullptr;
        if(buffer&&DestroyBuffer)DestroyBuffer(device,buffer,nullptr);buffer=VK_NULL_HANDLE;
        if(memory&&FreeMemory)FreeMemory(device,memory,nullptr);memory=VK_NULL_HANDLE;
        if(chain&&destroy_chain)ok(destroy_chain(chain),"destroy preview swapchain");chain=XR_NULL_HANDLE;
        logf("xr: flat quad preview closed; quad frames %llu, game images %llu",(unsigned long long)quad_frames,(unsigned long long)game_updates);
        return true;
    }
};
#undef PREVIEW_VK_FUNCTIONS
} }