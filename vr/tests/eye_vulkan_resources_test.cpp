// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "eye_vulkan_resources.h"
#include <iostream>
#include <set>
#include <stdexcept>
#include <type_traits>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T handle(uint64_t id){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(id));else return T(id);}
template<class T>uint64_t id(T h){if constexpr(std::is_pointer_v<T>)return uintptr_t(h);else return uint64_t(h);}
struct Fake {
    uint64_t next=100;int step=0,fail=0,submissions=0,waits=0;bool complete=false;
    VkResult submit_result=VK_SUCCESS;std::set<uint64_t> buffers,images,memory,fences;
    bool advance(){return ++step==fail;}
    bool empty()const{return buffers.empty()&&images.empty()&&memory.empty()&&fences.empty();}
} fake;
VKAPI_ATTR VkResult VKAPI_CALL create_buffer(VkDevice,const VkBufferCreateInfo* info,const VkAllocationCallbacks*,VkBuffer* out){
    if(fake.advance())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    check(info->size==1024&&!(info->flags&VK_BUFFER_CREATE_SPARSE_BINDING_BIT),"bounded non-sparse buffer");
    *out=handle<VkBuffer>(++fake.next);fake.buffers.insert(id(*out));return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_buffer(VkDevice,VkBuffer h,const VkAllocationCallbacks*){check(fake.buffers.erase(id(h))==1,"buffer destroyed once");}
VKAPI_ATTR void VKAPI_CALL buffer_requirements(VkDevice,VkBuffer,VkMemoryRequirements* r){*r={1024,256,1};}
VKAPI_ATTR VkResult VKAPI_CALL bind_buffer(VkDevice,VkBuffer,VkDeviceMemory,VkDeviceSize offset){check(offset==0,"dedicated bind");return fake.advance()?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL create_image(VkDevice,const VkImageCreateInfo* info,const VkAllocationCallbacks*,VkImage* out){
    if(fake.advance())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    check(info->extent.width==32&&info->extent.height==32&&info->arrayLayers==1&&info->samples==VK_SAMPLE_COUNT_1_BIT&&info->initialLayout==VK_IMAGE_LAYOUT_UNDEFINED,"explicit uninitialized image");
    *out=handle<VkImage>(++fake.next);fake.images.insert(id(*out));return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_image(VkDevice,VkImage h,const VkAllocationCallbacks*){check(fake.images.erase(id(h))==1,"image destroyed once");}
VKAPI_ATTR void VKAPI_CALL image_requirements(VkDevice,VkImage,VkMemoryRequirements* r){*r={4096,256,1};}
VKAPI_ATTR VkResult VKAPI_CALL bind_image(VkDevice,VkImage,VkDeviceMemory,VkDeviceSize offset){check(offset==0,"dedicated image bind");return fake.advance()?VK_ERROR_OUT_OF_DEVICE_MEMORY:VK_SUCCESS;}
VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice,const VkMemoryAllocateInfo* info,const VkAllocationCallbacks*,VkDeviceMemory* out){
    if(fake.advance())return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    check(info->memoryTypeIndex==0,"compatible device-local memory type");*out=handle<VkDeviceMemory>(++fake.next);fake.memory.insert(id(*out));return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice,VkDeviceMemory h,const VkAllocationCallbacks*){check(fake.memory.erase(id(h))==1,"allocation freed once");}
VKAPI_ATTR VkResult VKAPI_CALL create_fence(VkDevice,const VkFenceCreateInfo* info,const VkAllocationCallbacks*,VkFence* out){
    if(fake.advance())return VK_ERROR_OUT_OF_DEVICE_MEMORY;check(!info->flags,"initially unsignaled fence");
    *out=handle<VkFence>(++fake.next);fake.fences.insert(id(*out));return VK_SUCCESS;
}
VKAPI_ATTR void VKAPI_CALL destroy_fence(VkDevice,VkFence h,const VkAllocationCallbacks*){check(fake.fences.erase(id(h))==1,"fence destroyed once");}
VKAPI_ATTR VkResult VKAPI_CALL submit(VkQueue,uint32_t n,const VkSubmitInfo* info,VkFence fence){check(n==1&&info->commandBufferCount==1&&fake.fences.count(id(fence)),"single owned fence submission");++fake.submissions;return fake.submit_result;}
VKAPI_ATTR VkResult VKAPI_CALL wait(VkDevice,uint32_t n,const VkFence* fence,VkBool32 all,uint64_t){check(n==1&&all&&fake.fences.count(id(*fence)),"owned completion query");++fake.waits;return fake.complete?VK_SUCCESS:VK_TIMEOUT;}
EyeResourceApi api(){return {create_buffer,destroy_buffer,buffer_requirements,bind_buffer,create_image,destroy_image,image_requirements,bind_image,allocate,free_memory,create_fence,destroy_fence,submit,wait};}
VkPhysicalDeviceMemoryProperties properties(){VkPhysicalDeviceMemoryProperties p{};p.memoryTypeCount=1;p.memoryTypes[0]={VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,0};p.memoryHeapCount=1;p.memoryHeaps[0]={1024*1024,VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};return p;}
bool create(EyeResourcePair& p,uint64_t budget=10240){return p.create(handle<VkDevice>(1),api(),properties(),32,32,VK_FORMAT_R8G8B8A8_UNORM,1024,budget);}
int main(){try {
    // Every individual creation/allocation/bind/fence failure must unwind its prefix.
    for(int fail=1;fail<=13;++fail){fake={};fake.fail=fail;{EyeResourcePair p;check(!create(p),"injected allocation failure");check(!p.eye(0)&&!p.allocated_bytes(),"no partial publication");}check(fake.empty(),"failure cleans complete prefix");}
    fake={};{EyeResourcePair p;check(!create(p,10239),"actual requirements enforce pair budget");}check(fake.empty(),"budget failure cleanup");
    fake={};{EyeResourcePair p;auto props=properties();props.memoryTypes[0].propertyFlags=0;check(!p.create(handle<VkDevice>(1),api(),props,32,32,VK_FORMAT_R8G8B8A8_UNORM,1024,10240),"no compatible device-local type");}check(fake.empty(),"memory-type failure cleanup");
    fake={};{EyeResourcePair p;check(create(p),"pair created");check(p.allocated_bytes()==10240&&p.eye(0)&&p.eye(1)&&!p.eye(2),"bounded published resources");
        check(p.eye(0)->image!=p.eye(1)->image&&p.eye(0)->buffer!=p.eye(1)->buffer&&p.eye(0)->image_memory!=p.eye(1)->image_memory&&p.eye(0)->buffer_memory!=p.eye(1)->buffer_memory,"eyes have separate handles and allocations");
        check(p.submit(handle<VkQueue>(2),handle<VkCommandBuffer>(3))==VK_SUCCESS&&p.pending(),"submission pending");
        check(!p.poll(0)&&fake.memory.size()==4,"timeout retains resources");fake.complete=true;check(p.poll(0)&&!p.pending(),"completion retires pending flag");
        check(p.submit(handle<VkQueue>(2),handle<VkCommandBuffer>(3))!=VK_SUCCESS&&fake.submissions==1,"no accidental second replay");}
    check(fake.empty(),"completed resources freed");
    fake={};{EyeResourcePair p;check(create(p),"pending pair");check(p.submit(handle<VkQueue>(2),handle<VkCommandBuffer>(3))==VK_SUCCESS,"pending submit");}
    check(fake.memory.size()==4&&fake.images.size()==2&&fake.buffers.size()==2&&fake.fences.size()==1&&fake.waits==1,"uncertain completion retains all resources for device teardown");
    fake={};fake.submit_result=VK_ERROR_DEVICE_LOST;{EyeResourcePair p;check(create(p),"lost pair");check(p.submit(handle<VkQueue>(2),handle<VkCommandBuffer>(3))==VK_ERROR_DEVICE_LOST&&p.pending(),"device loss is uncertain");}
    check(fake.memory.size()==4,"device loss cannot free uncertain resources");
    fake={};fake.submit_result=VK_ERROR_OUT_OF_HOST_MEMORY;{EyeResourcePair p;check(create(p),"failed submit pair");check(p.submit(handle<VkQueue>(2),handle<VkCommandBuffer>(3))!=VK_SUCCESS&&!p.pending(),"unsubmitted OOM resources remain owned");}check(fake.empty(),"unsubmitted resources freed");
    std::cout<<"Owned Vulkan eye allocations: failure cleanup, separate memory, budget and fence lifetime checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
