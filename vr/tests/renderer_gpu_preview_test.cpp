// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace mkwvk {void logf(const char* fmt,...){va_list args;va_start(args,fmt);std::vfprintf(stderr,fmt,args);std::fputc('\n',stderr);va_end(args);}void gpu_test_feed_drop_owner();bool gpu_test_feed_alive();}
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
std::mutex queue;
bool locked(){bool result=false;std::thread thread([&]{if(queue.try_lock())queue.unlock();else result=true;});thread.join();return result;}
void env(const char* key,const char* value){
#ifdef _WIN32
    _putenv_s(key,value);
#else
    setenv(key,value,1);
#endif
}
int main(int argc,char** argv){using namespace mkwvk;try{
    check(argc==2,"fake loader path");env("HOOKSHOT_OPENXR_LOADER",argv[1]);env("RX_VK_XR_GPU_WORLD_PREVIEW","1");env("HOOKSHOT_VR_GPU_EYE_WIDTH","8");env("HOOKSHOT_VR_GPU_EYE_HEIGHT","8");
#ifdef _WIN32
    auto lib=LoadLibraryA(argv[1]);
#define SYMBOL(n) GetProcAddress(lib,n)
#else
    void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
#define SYMBOL(n) dlsym(lib,n)
#endif
    auto count=reinterpret_cast<int(*)(int)>(SYMBOL("xrTestCount"));auto set_check=reinterpret_cast<void(*)(bool(*)())>(SYMBOL("xrTestSetQueueCheck"));auto get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(SYMBOL("xrTestVulkanGetInstanceProcAddr"));check(count&&set_check&&get,"fake entry points");set_check(locked);
    check(xr_vulkan_prepare(true),"prepare adapter");VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
    check(xr_vulkan_instance(get,ci,&instance)&&xr_vulkan_physical(instance,&physical),"fake instance and GPU");VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};check(xr_vulkan_device(get,physical,di,&device),"fake device");
    const std::string mode=std::getenv("XR_FAKE_MODE");auto is=[&](const char* suffix){return mode==std::string("preview_pair_")+suffix;};const bool init_error=is("no_format")||is("pool_error");
    check(xr_vulkan_session_start(instance,physical,device,7,queue,1000,true)!=init_error,"GPU session startup");
    if(!init_error){std::this_thread::sleep_for(std::chrono::milliseconds(70));check(!xr_preview_wants_frame(),"GPU world never requests old CPU preview images");}
    gpu_test_feed_drop_owner();const bool retained=is("gpu_timeout")||is("submit_error")||is("image_timeout")||is("release_error");
    if(retained)check(gpu_test_feed_alive(),"pending/uncertain image work keeps the producer frame lease alive");
    check(xr_vulkan_session_stop(),"stop fake session owner");
    if(retained){check(count(11)==0&&count(1)==0&&gpu_test_feed_alive(),"uncertain completion retains swapchain/session and source frame");}
    else{check(count(0)==count(1)&&count(6)==count(7)&&count(10)==count(11),"complete or partial GPU presentation resources cleaned");check(!gpu_test_feed_alive(),"completed copy drops producer frame lease");}
    const bool rejected=is("missing")||is("stale")||is("synthetic")||is("bad_eye")||is("wrong_origin")||is("wrong_device")||is("wrong_size");
    if(rejected)check(count(12)==0&&count(17)==0,"invalid/unavailable paired frames rejected before XR image acquisition");
    if(!init_error&&!retained&&!rejected&&!is("invalid_index")&&!is("origin_pending")){check(count(19)>0&&count(20)==0&&count(14)==1&&count(12)==1&&count(13)==1,"reuse one released stereo image, preserving its old production pose/FOV");}
    if(is("origin_pending"))check(count(19)==0&&count(13)==1,"old-origin in-flight copy may retire but never becomes a new-origin layer");
    if(is("gpu_deferred")||is("origin_pending"))check(count(23)>=4,"GPU timeout is polled without premature release");if(is("image_deferred"))check(count(24)>=2,"XR image wait is retried before copying");
    check(count(15)==0&&count(16)==0&&count(18)==0,"no CPU pixel staging buffer, mapping or flush");check(count(4)==0&&count(5)==0&&count(2)==count(3),"balanced XR frames, queue locking, exact buffer regions and production metadata");xr_vulkan_reset();
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());xr_vulkan_reset();return 1;}}
