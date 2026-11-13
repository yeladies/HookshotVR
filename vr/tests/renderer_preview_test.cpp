// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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
namespace mkwvk {void logf(const char* fmt,...){va_list a;va_start(a,fmt);std::vfprintf(stderr,fmt,a);std::fputc('\n',stderr);va_end(a);}}
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
std::mutex queue;
bool locked(){bool result=false;std::thread t([&]{if(queue.try_lock())queue.unlock();else result=true;});t.join();return result;}
int main(int argc,char** argv){using namespace mkwvk;try {
    check(argc==2,"fake path");
    const char* initial_mode=std::getenv("XR_FAKE_MODE");
    const bool immersive=initial_mode&&std::strstr(initial_mode,"preview_immersive");
    const bool missing=initial_mode&&!std::strcmp(initial_mode,"preview_immersive_missing");
    if(immersive){
#ifdef _WIN32
        _putenv_s("RX_VK_XR_IMMERSIVE","1");
#else
        setenv("RX_VK_XR_IMMERSIVE","1",1);
#endif
    }

#ifdef _WIN32
    _putenv_s("HOOKSHOT_OPENXR_LOADER",argv[1]);HMODULE lib=LoadLibraryA(argv[1]);
#define SYM(n) GetProcAddress(lib,n)
#else
    setenv("HOOKSHOT_OPENXR_LOADER",argv[1],1);void* lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
#define SYM(n) dlsym(lib,n)
#endif
    auto count=reinterpret_cast<int(*)(int)>(SYM("xrTestCount"));
    auto set_check=reinterpret_cast<void(*)(bool(*)())>(SYM("xrTestSetQueueCheck"));
    auto get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(SYM("xrTestVulkanGetInstanceProcAddr"));
    check(count&&set_check&&get,"fake entry points");set_check(locked);
    check(xr_vulkan_prepare(true),"prepare");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
    check(xr_vulkan_instance(get,ci,&instance)&&xr_vulkan_physical(instance,&physical),"instance/GPU");
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};check(xr_vulkan_device(get,physical,di,&device),"device");
    const char* mode=std::getenv("XR_FAKE_MODE");
    const bool init_error=!std::strcmp(mode,"preview_no_format")||!std::strcmp(mode,"preview_allocation_error");
    const bool runtime_error=!std::strcmp(mode,"preview_invalid_index")||!std::strcmp(mode,"preview_submit_error")||!std::strcmp(mode,"preview_image_timeout")||!std::strcmp(mode,"preview_gpu_timeout");
    check(xr_vulkan_session_start(instance,physical,device,7,queue,1000,true)!=init_error,"preview startup");
    if(!init_error) {
        auto pixels=std::make_shared<std::vector<uint8_t>>(1280*720*4,0);
        (*pixels)[0]=17;(*pixels)[1]=34;(*pixels)[2]=51;(*pixels)[3]=255;
        if(immersive&&!missing){
            XrFrameSnapshot snapshot;bool ready=false;for(int i=0;i<100;++i){if(xr_vulkan_latest_frame(&snapshot)&&snapshot.focused){ready=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(2));}check(ready,"fresh fake head pose");
            float view[12]={1,0,0,0,0,1,0,0,0,0,1,0};check(xr_immersive_head_camera(7,view),"real pose camera callback");
            float block[0x230/4]{};std::memcpy(block,view,sizeof view);const float projection[16]={1,0,0,0,0,2,0,0,0,0,-1,-.2f,0,0,-1,0};std::memcpy(block+0x70/4,projection,sizeof projection);
            xr_immersive_note_camera(77,block,0x230/4);xr_immersive_tag_present(12,77);
        }
        xr_preview_publish(pixels,1280,720,12);
        bool reached=false;
        for(int i=0;i<1000;++i) {
            if((missing?count(3)>=3:(runtime_error?count(12)>0:count(17)>=2))){reached=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        check(reached,"preview work reached");
    }
    check(xr_vulkan_session_stop(),"stop owner");
    const bool retained=!std::strcmp(mode,"preview_gpu_timeout");
    if(retained)check(count(16)==0&&count(11)==0&&count(1)==0,"pending GPU keeps all referenced resources alive");
    else {
        check(count(0)==count(1)&&count(6)==count(7),"session and spaces cleaned");
        check(count(10)==count(11)&&count(15)==count(16),"preview partial/complete resources cleaned");
        if(missing)check(count(17)==0&&count(12)==0,"unassociated images never submitted as immersive layers");
        if(!init_error&&!runtime_error&&!missing)check(count(17)>=2&&count(14)==1&&count(12)==1,"reuse released image across quad frames without acquiring or uploading again");
    }
    if(!std::strcmp(mode,"preview_noncoherent"))check(count(18)>0,"noncoherent staging flush");
    check(count(4)==0&&count(5)==0,"frame balance, queue locking and copied color pattern");
    check(count(2)==count(3),"begun frames ended");
    xr_vulkan_reset();
#ifdef _WIN32
    FreeLibrary(lib);
#else
    dlclose(lib);
#endif
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());xr_vulkan_reset();return 1;}}