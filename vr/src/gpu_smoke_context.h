// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "eye_vulkan_resources.h"
#include <memory>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace hookshot::vr::smoke {
inline void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
inline void checked(VkResult r,const char* why){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(why)+": "+std::to_string(r));}
struct AddressBuffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;VkDeviceAddress address=0;uint64_t bytes=0;};
struct Context {
    PFN_vkGetInstanceProcAddr get=nullptr;PFN_vkGetDeviceProcAddr gd=nullptr;
    VkInstance instance=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;VkBuffer staging=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;
    std::array<AddressBuffer,4> auxiliary{};uint64_t auxiliary_bytes=0;
    VkDescriptorPool descriptor_pool=VK_NULL_HANDLE;VkDescriptorSetLayout descriptor_layout=VK_NULL_HANDLE;
    std::array<VkDescriptorSet,2> descriptor_sets{};
    VkShaderModule matrix_shader=VK_NULL_HANDLE;VkPipeline matrix_pipeline=VK_NULL_HANDLE;
    VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    VkShaderModule root_shader=VK_NULL_HANDLE;VkPipeline root_pipeline=VK_NULL_HANDLE;VkPipelineLayout root_layout=VK_NULL_HANDLE;
    bool uncertain=false;
    template<class T>T global(const char* name){auto fn=get(instance,name);require(fn!=nullptr,name);return reinterpret_cast<T>(fn);}
    template<class T>T fn(const char* name){auto p=gd(device,name);require(p!=nullptr,name);return reinterpret_cast<T>(p);}
    ~Context(){
        // On uncertain execution retain everything for process exit, including device/loader.
        if(uncertain)return;
        if(device){
            for(auto& buffer:auxiliary){
                if(buffer.buffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,buffer.buffer,nullptr);
                if(buffer.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,buffer.memory,nullptr);
            }
            if(descriptor_pool)fn<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(device,descriptor_pool,nullptr);
            if(root_pipeline)fn<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device,root_pipeline,nullptr);
            if(root_layout)fn<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device,root_layout,nullptr);
            if(root_shader)fn<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device,root_shader,nullptr);
            if(matrix_pipeline)fn<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device,matrix_pipeline,nullptr);
            if(matrix_shader)fn<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device,matrix_shader,nullptr);
            if(pipeline)fn<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device,pipeline,nullptr);
            if(layout)fn<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device,layout,nullptr);
            if(descriptor_layout)fn<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(device,descriptor_layout,nullptr);
            if(shader)fn<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device,shader,nullptr);
            if(pool)fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,pool,nullptr);
            if(staging)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,staging,nullptr);
            if(memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,memory,nullptr);
            fn<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
        }
        if(instance)global<PFN_vkDestroyInstance>("vkDestroyInstance")(instance,nullptr);
    }
};
inline void address_buffer(Context& c,const EyeResourceApi& api,const VkPhysicalDeviceMemoryProperties& memory,uint64_t bytes,AddressBuffer& out){
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;
    bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    checked(api.create_buffer(c.device,&bi,nullptr,&out.buffer),"create auxiliary address buffer");
    VkMemoryRequirements req{};api.buffer_requirements(c.device,out.buffer,&req);
    require(req.size&&req.size<=96ull*1024*1024-c.auxiliary_bytes,"auxiliary actual-allocation budget");
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}
    require(type!=UINT32_MAX,"auxiliary device-local memory required");
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&flags;ai.allocationSize=req.size;ai.memoryTypeIndex=type;
    checked(api.allocate(c.device,&ai,nullptr,&out.memory),"allocate auxiliary address storage");c.auxiliary_bytes+=req.size;
    checked(api.bind_buffer(c.device,out.buffer,out.memory,0),"bind auxiliary address storage");
    VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=out.buffer;
    out.address=api.buffer_address(c.device,&info);out.bytes=bytes;
    require(out.address&&out.address%16==0&&out.address<=UINT64_MAX-bytes,"valid auxiliary device address");
}
struct SmokeDevice {VkPhysicalDevice physical;uint32_t family;VkPhysicalDeviceProperties props;VkPhysicalDeviceMemoryProperties memory;EyeResourceApi api;};
inline SmokeDevice initialize_smoke(Context& c){
#ifdef _WIN32
        auto library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
#else
        auto library=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library,"vkGetInstanceProcAddr"));
#endif
        require(c.get,"System Vulkan loader unavailable");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="HookshotVR private-eye resource smoke";app.apiVersion=VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        checked(c.global<PFN_vkCreateInstance>("vkCreateInstance")(&ici,nullptr,&c.instance),"create instance");
        auto enumerate=c.global<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");uint32_t count=0;checked(enumerate(c.instance,&count,nullptr),"enumerate count");require(count>0&&count<=32,"bounded physical devices");
        std::vector<VkPhysicalDevice> devices(count);checked(enumerate(c.instance,&count,devices.data()),"enumerate GPU");
        VkPhysicalDevice physical=VK_NULL_HANDLE;uint32_t family=0;
        for(auto candidate:devices){uint32_t n=0;auto queues=c.global<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");queues(candidate,&n,nullptr);require(n<=256,"bounded queue families");std::vector<VkQueueFamilyProperties> q(n);queues(candidate,&n,q.data());
            for(uint32_t i=0;i<n;++i)if(q[i].queueCount&&((q[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))){physical=candidate;family=i;break;}
            if(physical)break;
        }
        require(physical,"graphics queue unavailable");VkPhysicalDeviceProperties props{};c.global<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical,&props);
        VkPhysicalDeviceMemoryProperties memory{};c.global<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&memory);
        require(props.apiVersion>=VK_API_VERSION_1_2,"Vulkan 1.2 required for native device-address smoke");
        VkPhysicalDeviceBufferDeviceAddressFeatures address_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&address_features;
        c.global<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(physical,&features);
        require(address_features.bufferDeviceAddress&&features.features.shaderInt64,"buffer device address / shader int64 unavailable");
        address_features.bufferDeviceAddressCaptureReplay=VK_FALSE;address_features.bufferDeviceAddressMultiDevice=VK_FALSE;
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkPhysicalDeviceFeatures enabled{};enabled.shaderInt64=VK_TRUE;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pEnabledFeatures=&enabled;di.pNext=&address_features;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        checked(c.global<PFN_vkCreateDevice>("vkCreateDevice")(physical,&di,nullptr,&c.device),"create device");
        c.gd=c.global<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");c.fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(c.device,family,0,&c.queue);
        EyeResourceApi api;
#define LOAD(field,name) api.field=c.fn<PFN_vk##name>("vk" #name)
        LOAD(create_buffer,CreateBuffer);LOAD(destroy_buffer,DestroyBuffer);LOAD(buffer_requirements,GetBufferMemoryRequirements);LOAD(bind_buffer,BindBufferMemory);
        LOAD(create_image,CreateImage);LOAD(destroy_image,DestroyImage);LOAD(image_requirements,GetImageMemoryRequirements);LOAD(bind_image,BindImageMemory);
        LOAD(allocate,AllocateMemory);LOAD(free,FreeMemory);LOAD(create_fence,CreateFence);LOAD(destroy_fence,DestroyFence);LOAD(submit,QueueSubmit);LOAD(wait,WaitForFences);LOAD(reset,ResetFences);
#undef LOAD
        api.buffer_address=c.fn<PFN_vkGetBufferDeviceAddress>("vkGetBufferDeviceAddress");
    return {physical,family,props,memory,api};
}
}
