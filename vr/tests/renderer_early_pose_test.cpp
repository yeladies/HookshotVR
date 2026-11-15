// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include "xr_camera_frame.h"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <thread>
#include <chrono>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace mkwvk {void logf(const char* fmt,...){va_list a;va_start(a,fmt);std::vfprintf(stderr,fmt,a);std::fputc('\n',stderr);va_end(a);}}
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static std::mutex queue;
static bool locked(){bool result=false;std::thread t([&]{if(queue.try_lock())queue.unlock();else result=true;});t.join();return result;}
int main(int argc,char** argv){using namespace mkwvk;
    void (*release_end)()=nullptr;
    try{check(argc==2,"loader path");
#ifdef _WIN32
        _putenv_s("HOOKSHOT_OPENXR_LOADER",argv[1]);_putenv_s("XR_FAKE_MODE","session_publish_before_end");
        auto library=LoadLibraryA(argv[1]);
        auto count=reinterpret_cast<int(*)(int)>(GetProcAddress(library,"xrTestCount"));
        auto set_check=reinterpret_cast<void(*)(bool(*)())>(GetProcAddress(library,"xrTestSetQueueCheck"));
        release_end=reinterpret_cast<void(*)()>(GetProcAddress(library,"xrTestReleaseEndFrame"));
#else
        setenv("HOOKSHOT_OPENXR_LOADER",argv[1],1);setenv("XR_FAKE_MODE","session_publish_before_end",1);
        auto library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        auto count=reinterpret_cast<int(*)(int)>(dlsym(library,"xrTestCount"));
        auto set_check=reinterpret_cast<void(*)(bool(*)())>(dlsym(library,"xrTestSetQueueCheck"));
        release_end=reinterpret_cast<void(*)()>(dlsym(library,"xrTestReleaseEndFrame"));
#endif
        check(count&&set_check&&release_end,"fake metrics");set_check(locked);check(xr_vulkan_prepare(true),"prepare");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
        VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};
        check(xr_vulkan_instance(nullptr,ci,&instance)&&xr_vulkan_physical(instance,&physical),"instance/device");
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};check(xr_vulkan_device(nullptr,physical,di,&device),"logical device");
        check(xr_vulkan_session_start(instance,physical,device,7,queue,1000),"session");
        for(unsigned i=0;i<500&&!count(30);++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(count(30)==1&&count(3)==0,"xrEndFrame entered but has not completed");
        XrFrameSnapshot snapshot;check(xr_vulkan_latest_frame(&snapshot),"located pose must be published before EndFrame returns");
        check(snapshot.sequence==1&&snapshot.predicted_display_time==10000000&&snapshot.head.flags,"coherent current frame");
        VrCameraRig rig{};rig.world_units_per_metre=1;rig.origin_generation=snapshot.origin_generation;VrCameraFrame camera;
        check(xr_camera_frame(snapshot,rig,.1,25000,camera)==VR_CAMERA_OK,"new camera consumes pre-submit located sample");
        check(camera.sequence==snapshot.sequence&&camera.predicted_display_time==snapshot.predicted_display_time,"time identity preserved");
        const auto observed=snapshot.observed_at_monotonic_ns;std::this_thread::sleep_for(std::chrono::milliseconds(5));
        check(xr_vulkan_latest_frame(&snapshot)&&snapshot.observed_at_monotonic_ns==observed,"waiting does not invent fresh pose timestamps");
        release_end();check(xr_vulkan_session_stop(),"stop");
        check(count(2)==count(3)&&count(4)==0&&count(5)==0,"balanced frame and queue synchronization");
        xr_vulkan_reset();std::puts("Located XR pose reaches new camera before xrEndFrame completion");return 0;
    }catch(const std::exception& e){if(release_end)release_end();xr_vulkan_reset();std::fprintf(stderr,"%s\n",e.what());return 1;}
}
