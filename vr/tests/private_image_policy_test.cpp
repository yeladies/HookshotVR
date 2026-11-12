// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace mkwvk {
using VkImage=uint64_t;using VmaAllocation=void*;
constexpr VkImage VK_NULL_HANDLE=0;
constexpr int VK_SUCCESS=0,VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE=1;
constexpr uint32_t VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT=1,VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT=2,VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT=4;
struct VkImageCreateInfo{};
struct VmaAllocationCreateInfo {int usage=0;uint32_t requiredFlags=0,flags=0;};
struct Device {bool memory_budget=false;} dev;
int vma=1,calls=0,result=0;VmaAllocationCreateInfo observed;
int vmaCreateImage(int,const VkImageCreateInfo*,const VmaAllocationCreateInfo* info,VkImage* image,VmaAllocation* allocation,void*){
    ++calls;observed=*info;*image=111;*allocation=reinterpret_cast<void*>(uintptr_t(222));return result;
}
#include "strict_private_image_function.inc"
}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {using namespace mkwvk;
    for(bool budget:{false,true})for(bool success:{false,true}) {
        dev.memory_budget=budget;calls=0;result=success?VK_SUCCESS:-1;
        VkImage image=17;VmaAllocation allocation=reinterpret_cast<void*>(uintptr_t(19));
        check(create_xr_private_image({},image,allocation)==success,"actual strict allocator result");
        check(calls==1,"no retry or fallback after allocation failure");
        check(observed.usage==VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE&&observed.requiredFlags==VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,"device-local memory required");
        check(observed.flags==(VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT|(budget?VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT:0)),"dedicated allocation with available driver budget check");
        check(success?(image==111&&allocation==reinterpret_cast<void*>(uintptr_t(222))):(image==17&&allocation==reinterpret_cast<void*>(uintptr_t(19))),"failed allocation cannot partially publish handles");
    }
    std::cout<<"Actual strict VMA image allocator has one local-only attempt and atomic outputs\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
