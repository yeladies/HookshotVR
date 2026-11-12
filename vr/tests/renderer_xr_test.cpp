// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace mkwvk {
void logf(const char* fmt,...) { va_list args;va_start(args,fmt);std::vfprintf(stderr,fmt,args);std::fputc('\n',stderr);va_end(args); }
}
static void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
static void loader(const char* path) {
#ifdef _WIN32
    _putenv_s("HOOKSHOT_OPENXR_LOADER",path);
#else
    setenv("HOOKSHOT_OPENXR_LOADER",path,1);
#endif
}
int main(int argc,char** argv) {
    using namespace mkwvk;
    try {
        check(argc==2,"loader argument");
        loader("");
        check(xr_vulkan_prepare(false)&&!xr_vulkan_active(),"flat mode needs no loader");
        check(!xr_vulkan_prepare(true),"missing loader fails closed");
        loader("relative-loader.dll");check(!xr_vulkan_prepare(true),"relative loader rejected");
        loader(argv[1]);
        const char* value=std::getenv("XR_FAKE_MODE");const char* mode=value?value:"success";
        const bool prepare_error=!std::strcmp(mode,"no_extension")||!std::strcmp(mode,"no_headset")||
            !std::strcmp(mode,"range")||!std::strcmp(mode,"missing_function");
        const bool prepared=xr_vulkan_prepare(true);
        check(prepared!=prepare_error,"prepare result");
        if(prepared) {
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};info.pApplicationInfo=&app;
            VkInstance instance=VK_NULL_HANDLE;
            const bool instance_error=!std::strcmp(mode,"vk_instance_error")||!std::strcmp(mode,"xr_instance_error");
            check(xr_vulkan_instance(nullptr,info,&instance)!=instance_error,"dual instance status");
            if(!instance_error) {
                VkPhysicalDevice physical=VK_NULL_HANDLE;
                check(xr_vulkan_physical(instance,&physical)&&physical!=VK_NULL_HANDLE,"runtime physical GPU");
                VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
                VkDevice device=VK_NULL_HANDLE;
                const bool device_error=!std::strcmp(mode,"vk_device_error")||!std::strcmp(mode,"xr_device_error");
                check(xr_vulkan_device(nullptr,physical,device_info,&device)!=device_error,"dual device status");
            }
        }
        xr_vulkan_reset();check(!xr_vulkan_active(),"reset clears readiness");
        check(xr_vulkan_prepare(false),"retry flat after reset");
        return 0;
    } catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}