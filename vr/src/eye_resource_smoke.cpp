// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit opt-in native GPU smoke. No game, XR runtime, window or display changes.
#define VK_NO_PROTOTYPES
#include "eye_vulkan_resources.h"
#include "eye_uniform_copy.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
using namespace hookshot::vr;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void checked(VkResult r,const char* why){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(why)+": "+std::to_string(r));}
struct Context {
    PFN_vkGetInstanceProcAddr get=nullptr;PFN_vkGetDeviceProcAddr gd=nullptr;
    VkInstance instance=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;VkBuffer staging=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;
    bool uncertain=false;
    template<class T>T global(const char* name){auto fn=get(instance,name);require(fn!=nullptr,name);return reinterpret_cast<T>(fn);}
    template<class T>T fn(const char* name){auto p=gd(device,name);require(p!=nullptr,name);return reinterpret_cast<T>(p);}
    ~Context(){
        // On uncertain execution retain everything for process exit, including device/loader.
        if(uncertain)return;
        if(device){
            if(pool)fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,pool,nullptr);
            if(staging)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,staging,nullptr);
            if(memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,memory,nullptr);
            fn<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
        }
        if(instance)global<PFN_vkDestroyInstance>("vkDestroyInstance")(instance,nullptr);
    }
};
int main(int argc,char** argv){
    if(argc!=2||std::strcmp(argv[1],"--run")){std::cerr<<"Explicit native GPU opt-in required: --run\n";return 2;}
    try {
        Context c;
#ifdef _WIN32
        auto library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
#else
        auto library=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library,"vkGetInstanceProcAddr"));
#endif
        require(c.get,"System Vulkan loader unavailable");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="HookshotVR private-eye resource smoke";app.apiVersion=VK_API_VERSION_1_0;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        checked(c.global<PFN_vkCreateInstance>("vkCreateInstance")(&ici,nullptr,&c.instance),"create instance");
        auto enumerate=c.global<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");uint32_t count=0;checked(enumerate(c.instance,&count,nullptr),"enumerate count");require(count>0&&count<=32,"bounded physical devices");
        std::vector<VkPhysicalDevice> devices(count);checked(enumerate(c.instance,&count,devices.data()),"enumerate GPU");
        VkPhysicalDevice physical=VK_NULL_HANDLE;uint32_t family=0;
        for(auto candidate:devices){uint32_t n=0;auto queues=c.global<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");queues(candidate,&n,nullptr);require(n<=256,"bounded queue families");std::vector<VkQueueFamilyProperties> q(n);queues(candidate,&n,q.data());
            for(uint32_t i=0;i<n;++i)if(q[i].queueCount&&(q[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){physical=candidate;family=i;break;}
            if(physical)break;
        }
        require(physical,"graphics queue unavailable");VkPhysicalDeviceProperties props{};c.global<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical,&props);
        VkPhysicalDeviceMemoryProperties memory{};c.global<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&memory);
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        checked(c.global<PFN_vkCreateDevice>("vkCreateDevice")(physical,&di,nullptr,&c.device),"create device");
        c.gd=c.global<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");c.fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(c.device,family,0,&c.queue);
        EyeResourceApi api;
#define LOAD(field,name) api.field=c.fn<PFN_vk##name>("vk" #name)
        LOAD(create_buffer,CreateBuffer);LOAD(destroy_buffer,DestroyBuffer);LOAD(buffer_requirements,GetBufferMemoryRequirements);LOAD(bind_buffer,BindBufferMemory);
        LOAD(create_image,CreateImage);LOAD(destroy_image,DestroyImage);LOAD(image_requirements,GetImageMemoryRequirements);LOAD(bind_image,BindImageMemory);
        LOAD(allocate,AllocateMemory);LOAD(free,FreeMemory);LOAD(create_fence,CreateFence);LOAD(destroy_fence,DestroyFence);LOAD(submit,QueueSubmit);LOAD(wait,WaitForFences);LOAD(reset,ResetFences);
#undef LOAD
        // Entirely authored camera payload: no game data or measured world calibration.
        std::vector<std::byte> original(1024);
        for(size_t i=0;i<original.size();++i)original[i]=std::byte((i*37)%256);
        float base[0xe0/4]{};for(int i=0;i<3;++i)base[4*i+i]=1;
        ProjectionMatrix projection{};require(eye_projection({-.7,.8,.9,-.6},.05,1000,ClipDepth::MinusOneToOne,DepthOrder::Forward,false,projection),"authored projection");
        for(int i=0;i<16;++i){base[0x30/4+i]=float(projection[i]);base[0x70/4+i]=float(projection[i]);}
        uniform_detail::inverse(base,base+0xb0/4);std::memcpy(original.data(),base,sizeof base);
        const auto source_before=original;std::array<std::vector<std::byte>,2> payloads;
        for(unsigned eye=0;eye<2;++eye){
            EyeMatrices matrices{};TrackingOrigin origin{};origin.yaw_radians=.3;
            XrPosef pose{{0,0,float(std::sin(.1)),float(std::cos(.1))},{eye?.032f:-.032f,1,2}};
            require(tracked_camera_view(origin,pose,XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,matrices.view),"authored tracked eye");
            matrices.projection=projection;
            for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)
                matrices.world_to_clip[4*r+col]+=projection[4*r+k]*(k==3?(col==3?1:0):matrices.view.rows[k][col]);
            require(prepare_eye_uniform_copy(original.data(),original.size(),matrices,ClipDepth::MinusOneToOne,payloads[eye]),"prepare complete eye payload");
        }
        require(original==source_before&&payloads[0]!=payloads[1],"source preserved and eye payloads distinct");
        EyeResourcePair pair;require(pair.create(c.device,api,memory,32,32,VK_FORMAT_R8G8B8A8_UNORM,1024,8*1024*1024),"create bounded independent eyes");
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=2*(32*32*4+1024);bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(api.create_buffer(c.device,&bi,nullptr,&c.staging),"create readback");VkMemoryRequirements req{};api.buffer_requirements(c.device,c.staging,&req);require(req.size<=1024*1024,"bounded staging allocation");
        uint32_t type=UINT32_MAX;bool coherent=false;
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){type=i;coherent=memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;break;}
        require(type!=UINT32_MAX,"host-visible readback memory");VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=type;checked(api.allocate(c.device,&ai,nullptr,&c.memory),"allocate readback");checked(api.bind_buffer(c.device,c.staging,c.memory,0),"bind readback");
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;checked(c.fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(c.device,&pi,nullptr,&c.pool),"command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=c.pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer command=VK_NULL_HANDLE;
        checked(c.fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(c.device,&ca,&command),"command buffer");for(unsigned round=0;round<2;++round){
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(c.fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(command,&begin),"begin");
        auto barrier=c.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        for(unsigned eye=0;eye<2;++eye){
            const auto& e=*pair.eye(eye);VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};ib.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ib.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;ib.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;ib.srcQueueFamilyIndex=ib.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;ib.image=e.image;ib.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            barrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&ib);
            VkClearColorValue color{};color.float32[(eye+round)%2]=1;color.float32[3]=1;
            c.fn<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(command,e.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&ib.subresourceRange);
            c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,e.buffer,0,1024,payloads[eye].data());
            ib.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ib.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;ib.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;ib.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&mb,0,nullptr,1,&ib);
            VkBufferImageCopy copy{};copy.bufferOffset=eye*5120;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={32,32,1};
            c.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,e.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.staging,1,&copy);
            VkBufferCopy bc{0,eye*5120+4096,1024};c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,e.buffer,c.staging,1,&bc);
        }
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        checked(c.fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command),"end");
        const auto submitted=pair.submit(c.queue,command);c.uncertain=pair.pending();checked(submitted,"submit eyes");
        require(pair.poll(5000000000ull),"GPU fence not complete within five seconds; resources retained");c.uncertain=false;
        void* mapped=nullptr;checked(c.fn<PFN_vkMapMemory>("vkMapMemory")(c.device,c.memory,0,VK_WHOLE_SIZE,0,&mapped),"map readback");
        if(!coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=c.memory;range.size=VK_WHOLE_SIZE;checked(c.fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(c.device,1,&range),"invalidate readback");}
        bool correct=true;auto* bytes=static_cast<const unsigned char*>(mapped);
        for(unsigned eye=0;eye<2;++eye){for(unsigned pixel=0;pixel<1024;++pixel)for(unsigned channel=0;channel<4;++channel)correct&=bytes[eye*5120+pixel*4+channel]==((channel==(eye+round)%2||channel==3)?255:0);
            for(unsigned offset=0;offset<1024;++offset)correct&=bytes[eye*5120+4096+offset]==std::to_integer<unsigned char>(payloads[eye][offset]);}
        c.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(c.device,c.memory);require(correct,"eye image/buffer readback mismatch");
        if(round==0){
            require(pair.rearm(),"rearm only after completion");
            checked(c.fn<PFN_vkResetCommandPool>("vkResetCommandPool")(c.device,c.pool,0),"reset completed command pool");
            for(auto& payload:payloads)payload[0x300]^=std::byte{7};
        }
        }
        std::cout<<"GPU="<<props.deviceName<<"; eye_allocation_bytes="<<pair.allocated_bytes()<<"; independent_images=2; independent_buffers=2; image_pixels_checked=4096; buffer_bytes_checked=4096; full_uniform_payloads_checked=4; submissions=2; resource_allocations_reused=true; original_payload_unchanged=true; fence_complete=true; XR=false; game=false\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
