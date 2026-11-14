// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_gpu_scene_source.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <type_traits>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T handle(uint64_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(n));else return T(n);}
namespace mkwvk {
struct {VkDevice device=handle<VkDevice>(1);VkPhysicalDevice physical=handle<VkPhysicalDevice>(2);uint32_t queue_family=3;VkPhysicalDeviceFeatures features{};} dev;
bool par_active=false;
struct Texture {VkImage image=VK_NULL_HANDLE;void* allocation=nullptr;bool sparse=false;VkImageType type=VK_IMAGE_TYPE_2D;VkFormat format=VK_FORMAT_UNDEFINED;VkSampleCountFlagBits samples=VK_SAMPLE_COUNT_1_BIT;uint32_t depth=1,levels=1,layers=1,width=4,height=4;VkImageUsageFlags usage=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;VkImageAspectFlags aspect=0;};
struct Fake {uint64_t next=100,frame=1,completed=0,generation=1;int calls=0,fail=0;bool missing=false,probe=false;ImmersiveTag pose;bool synthetic=false;
    std::map<VkBuffer,uint64_t> sizes;std::map<VkBuffer,VkDeviceMemory> bindings;std::map<VkDeviceMemory,std::vector<unsigned char>> memory;std::map<VkImage,std::vector<unsigned char>> images;
} fake;
bool env_on(const char* name){return std::strcmp(name,"RX_VK_XR_GPU_SCENE_SOURCE")==0||(fake.probe&&std::strcmp(name,"RX_VK_XR_GPU_SCENE_SOURCE_PROBE")==0);}
uint64_t vk_timeline_completed(){return fake.completed;}
uint64_t texture_frame_no(){return fake.frame;}
uint64_t texture_destroy_generation(){return fake.generation;}
ImmersiveTag xr_immersive_camera_tag(const float*,size_t,bool* synthetic){*synthetic=fake.synthetic;return fake.pose;}
bool fail(){return ++fake.calls==fake.fail;}
VkResult vkCreateBuffer(VkDevice,const VkBufferCreateInfo* c,const VkAllocationCallbacks*,VkBuffer* out){if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkBuffer>(++fake.next);fake.sizes[*out]=c->size;return VK_SUCCESS;}
void vkDestroyBuffer(VkDevice,VkBuffer b,const VkAllocationCallbacks*){check(fake.sizes.erase(b)==1,"owned buffer");fake.bindings.erase(b);}
void vkGetBufferMemoryRequirements(VkDevice,VkBuffer b,VkMemoryRequirements* r){*r={fake.sizes.at(b),16,1};}
void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice,VkPhysicalDeviceMemoryProperties* m){*m={};m->memoryTypeCount=1;m->memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;}
VkResult vkAllocateMemory(VkDevice,const VkMemoryAllocateInfo* c,const VkAllocationCallbacks*,VkDeviceMemory* out){if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkDeviceMemory>(++fake.next);fake.memory[*out].resize(c->allocationSize);return VK_SUCCESS;}
void vkFreeMemory(VkDevice,VkDeviceMemory m,const VkAllocationCallbacks*){check(fake.memory.erase(m)==1,"owned memory");}
VkResult vkBindBufferMemory(VkDevice,VkBuffer b,VkDeviceMemory m,VkDeviceSize offset){check(offset==0,"zero offset");if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;fake.bindings[b]=m;return VK_SUCCESS;}
VkDeviceAddress vkGetBufferDeviceAddress(VkDevice,const VkBufferDeviceAddressInfo* b){return fail()?0:(uint64_t(reinterpret_cast<uintptr_t>(b->buffer))<<20);}
VkResult vkMapMemory(VkDevice,VkDeviceMemory m,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out){if(fail())return VK_ERROR_MEMORY_MAP_FAILED;*out=fake.memory.at(m).data();return VK_SUCCESS;}
VKAPI_ATTR void VKAPI_CALL unmap(VkDevice,VkDeviceMemory){}
VKAPI_ATTR VkResult VKAPI_CALL invalidate(VkDevice,uint32_t,const VkMappedMemoryRange*){return VK_SUCCESS;}
PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice,const char* name){if(fake.missing)return nullptr;if(std::strcmp(name,"vkUnmapMemory")==0)return reinterpret_cast<PFN_vkVoidFunction>(unmap);return reinterpret_cast<PFN_vkVoidFunction>(invalidate);}
void vkCmdPipelineBarrier2(VkCommandBuffer,const VkDependencyInfo*){}
void vkCmdCopyImageToBuffer(VkCommandBuffer,VkImage image,VkImageLayout layout,VkBuffer buffer,uint32_t count,const VkBufferImageCopy* copy){check(layout==VK_IMAGE_LAYOUT_GENERAL&&count==1,"reviewed copy shape");auto& target=fake.memory.at(fake.bindings.at(buffer));auto& source=fake.images.at(image);check(copy->bufferOffset+source.size()<=target.size(),"bounded copy");std::memcpy(target.data()+copy->bufferOffset,source.data(),source.size());}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer src,VkBuffer dst,uint32_t,const VkBufferCopy* c){auto& s=fake.memory.at(fake.bindings.at(src));auto& d=fake.memory.at(fake.bindings.at(dst));check(c->srcOffset+c->size<=s.size()&&c->dstOffset+c->size<=d.size(),"bounded readback");std::memcpy(d.data()+c->dstOffset,s.data()+c->srcOffset,c->size);}
}
#include "gpu_scene_source_function.inc"
using namespace mkwvk;
Texture color,depth;std::array<float,140> camera;
void reset(){auto& p=gpu_scene_pool();p.~GpuScenePool();new(&p)GpuScenePool;fake={};dev.device=handle<VkDevice>(1);dev.features.shaderInt64=VK_TRUE;par_active=false;
    color={};color.image=handle<VkImage>(10);color.allocation=handle<void*>(20);color.format=VK_FORMAT_B10G11R11_UFLOAT_PACK32;color.aspect=VK_IMAGE_ASPECT_COLOR_BIT;
    depth=color;depth.image=handle<VkImage>(11);depth.allocation=handle<void*>(21);depth.format=VK_FORMAT_D32_SFLOAT;depth.aspect=VK_IMAGE_ASPECT_DEPTH_BIT;
    fake.images[color.image]=std::vector<unsigned char>(64,19);fake.images[depth.image]=std::vector<unsigned char>(64,43);
    camera={};for(int i=0;i<3;++i)camera[4*i+i]=1;camera[28]=camera[33]=1;camera[38]=-1.002f;camera[39]=-.2002f;camera[42]=-1;std::copy(camera.begin()+28,camera.begin()+44,camera.begin()+12);
}
void capture(uint64_t frame,uint64_t recording=0){fake.frame=frame;xr_gpu_scene_source_tick(frame);xr_gpu_scene_source_note(&color,&depth,camera.data(),camera.size());xr_gpu_scene_source_view(&color,VK_FORMAT_E5B9G9R9_UFLOAT_PACK32);xr_gpu_scene_source_network(handle<VkCommandBuffer>(3),recording?recording:frame*10,0xfd029a317a9a4b94ull,{&color});}
auto latest(){return xr_gpu_scene_source_acquire(dev.device,0,gpu_scene_now(),false);}
void complete(uint64_t frame){fake.completed=frame*10;fake.frame=frame+1;xr_gpu_scene_source_tick(frame+1);}
int main(){try{
    reset();check(xr_gpu_scene_source_stop(),"empty stop");
    for(int i=1;i<=4;++i){reset();fake.fail=i;capture(1);check(!latest()&&fake.sizes.empty()&&fake.memory.empty(),"each partial source allocation unwinds without commands");}
    for(int i=1;i<=4;++i){reset();capture(1);complete(1);auto& pool=gpu_scene_pool();const auto bytes=pool.allocated;fake.fail=fake.calls+i;GpuSceneBuffer probe;check(!gpu_scene_allocate(pool,128,true,probe)&&pool.allocated==bytes&&fake.memory.size()==1&&fake.sizes.size()==1,"host readback allocation/map failures unwind separately");check(xr_gpu_scene_source_stop(),"failed readback leaves original packet releasable");}
    reset();fake.missing=true;capture(1);check(fake.calls==0&&gpu_scene_pool().stopped,"missing device function stops before allocation");
    reset();camera[0]=2;capture(1);check(!fake.calls,"nonrigid camera rejected");
    {reset();capture(1);check(!latest()&&gpu_scene_pool().captures==1,"no publication before actual timeline completion");complete(1);auto a=latest();check(a&&a->state==1&&a->bytes==128&&a->queue_family==3,"completed immutable packet publishes");
    const auto source=fake.memory.at(fake.bindings.at(a->buffer));for(unsigned i=0;i<128;++i)check(source[i]==(i<64?19:43),"contiguous original color/depth packet");
    const auto saved=*a;capture(2);complete(2);auto b=latest();capture(3);complete(3);auto c=latest();check(b&&c&&a->state==1&&b->state==2&&c->state==3,"metadata never rewritten for old leases");capture(4);check(gpu_scene_pool().captures==3&&gpu_scene_pool().leased_skips==1,"three leased slots apply backpressure");
    a.reset();capture(5);check(gpu_scene_pool().captures==3,"copied value retains private pin");b.reset();capture(6);complete(6);check(gpu_scene_pool().captures==4&&saved.state==1&&saved.buffer!=latest()->buffer,"only genuinely unpinned slot reused");
    check(!xr_gpu_scene_source_acquire(dev.device,0,gpu_scene_now()+2000000000,false),"stale packet rejected");check(!xr_gpu_scene_source_acquire(handle<VkDevice>(99),0,gpu_scene_now(),false),"wrong device rejected");check(!xr_gpu_scene_source_acquire(dev.device,1,gpu_scene_now()),"invalid pose cannot enter XR");
    // Weak observers cannot resurrect metadata after the owner detached it.
    c.reset();auto current=latest();std::weak_ptr<const XrGpuSceneSource> weak=current;current.reset();capture(7);complete(7);capture(8);complete(8);check(weak.expired(),"old weak metadata expires on reuse");
    } // All source leases, including copied values, are gone before fixture reset.
    reset();fake.pose.valid=true;fake.pose.generation=7;fake.pose.observed_ns=gpu_scene_now();capture(1);complete(1);check(bool(xr_gpu_scene_source_acquire(dev.device,7,gpu_scene_now())),"tracked mono pose accepted");check(!xr_gpu_scene_source_acquire(dev.device,8,gpu_scene_now()),"origin mismatch rejected");
    reset();fake.pose.valid=true;fake.pose.generation=7;fake.pose.observed_ns=gpu_scene_now();fake.pose.temporal_stereo=true;capture(1);complete(1);check(!xr_gpu_scene_source_acquire(dev.device,7,gpu_scene_now()),"temporal source cannot masquerade as shared mono scene");
    reset();fake.pose.valid=true;fake.pose.generation=7;fake.pose.observed_ns=gpu_scene_now();fake.synthetic=true;capture(1);complete(1);check(!xr_gpu_scene_source_acquire(dev.device,7,gpu_scene_now())&&xr_gpu_scene_source_acquire(dev.device,7,gpu_scene_now(),true,true),"synthetic pose needs explicit diagnostic opt in");
    reset();capture(1);check(!xr_gpu_scene_source_stop()&&!fake.memory.empty(),"stop retains pending GPU use");fake.completed=10;check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"known completion permits cleanup");
    reset();capture(1);complete(1);auto borrowed=latest();check(!xr_gpu_scene_source_stop()&&!fake.memory.empty(),"stop retains borrowed source");borrowed.reset();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"last borrower release permits cleanup");
    reset();capture(1);complete(1);dev.device=handle<VkDevice>(2);check(!xr_gpu_scene_source_acquire(handle<VkDevice>(1),0,gpu_scene_now(),false),"acquisition itself observes abandoned device");dev.device=handle<VkDevice>(1);check(!latest(),"acquisition also makes device abandonment permanent");
    reset();capture(1);complete(1);dev.device=handle<VkDevice>(2);xr_gpu_scene_source_tick(3);dev.device=handle<VkDevice>(1);check(!latest()&&!xr_gpu_scene_source_stop()&&!fake.memory.empty(),"device handle ABA never revives abandoned storage");
    std::cout<<"Actual GPU source pool: completion, immutable leases/copies, bounded backpressure, pose scope, allocation unwind and device abandonment passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
