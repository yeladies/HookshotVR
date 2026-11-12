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
namespace mkwvk { void logf(const char* fmt,...) {va_list a;va_start(a,fmt);std::vfprintf(stderr,fmt,a);std::fputc('\n',stderr);va_end(a);} }
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
std::mutex test_queue;
bool is_queue_locked() {
    bool locked=false;
    std::thread contender([&]{if(test_queue.try_lock())test_queue.unlock();else locked=true;});
    contender.join();return locked;
}
int main(int argc,char** argv) {
    using namespace mkwvk;
    try {
        check(argc==2,"loader path");
#ifdef _WIN32
        _putenv_s("HOOKSHOT_OPENXR_LOADER",argv[1]);
        HMODULE library=LoadLibraryA(argv[1]);
        auto count=reinterpret_cast<int(*)(int)>(GetProcAddress(library,"xrTestCount"));
        auto set_check=reinterpret_cast<void(*)(bool(*)())>(GetProcAddress(library,"xrTestSetQueueCheck"));
#else
        setenv("HOOKSHOT_OPENXR_LOADER",argv[1],1);
        void* library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
        auto count=reinterpret_cast<int(*)(int)>(dlsym(library,"xrTestCount"));
        auto set_check=reinterpret_cast<void(*)(bool(*)())>(dlsym(library,"xrTestSetQueueCheck"));
#endif
        check(count&&set_check,"fake metrics loaded");set_check(is_queue_locked);
        const char* value=std::getenv("XR_FAKE_MODE");const char* mode=value?value:"session_success";
        check(xr_vulkan_prepare(true),"prepare");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
        VkInstance instance=VK_NULL_HANDLE;VkPhysicalDevice physical=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;
        check(xr_vulkan_instance(nullptr,ci,&instance)&&xr_vulkan_physical(instance,&physical),"instance/GPU");
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};check(xr_vulkan_device(nullptr,physical,di,&device),"device");
        const bool init_error=!std::strcmp(mode,"session_create_error")||!std::strcmp(mode,"session_space_error")||
            !std::strcmp(mode,"session_wrong_stereo")||!std::strcmp(mode,"session_missing_blend")||!std::strcmp(mode,"session_missing_function");
        const bool terminal=!std::strcmp(mode,"session_instance_loss")||!std::strcmp(mode,"session_stop")||!std::strcmp(mode,"session_loss")||
            !std::strcmp(mode,"session_wait_error")||!std::strcmp(mode,"session_begin_error")||
            !std::strcmp(mode,"session_end_error")||!std::strcmp(mode,"session_locate_error");
        check(xr_vulkan_session_start(instance,physical,device,7,test_queue)!=init_error,"session startup outcome");
        XrFrameSnapshot frame{};bool received=false;
        if(!init_error) {
            for(int i=0;i<1000;++i) {
                if(xr_vulkan_latest_frame(&frame)&&frame.sequence>=uint64_t(!std::strcmp(mode,"session_stale")?1:3)){received=true;if(!terminal)break;}
                if(terminal&&count(1)>0)break;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if(terminal)check(count(1)>0,"terminal path destroys session");
            else {
                check(received&&frame.predicted_display_time>0&&frame.observed_at_monotonic_ns>0&&frame.origin_generation>=1,"complete predicted-time sample");
                check(frame.focused==bool(std::strcmp(mode,"session_focus_loss")),"focus state loss preserved");
                if(!std::strcmp(mode,"session_no_render"))check(!frame.should_render,"shouldRender false preserved");
                else check(frame.should_render,"shouldRender true preserved");
                if(!std::strcmp(mode,"session_invalid_pose"))check(!frame.head.flags&&!frame.eyes[0].pose.flags,"invalid flags preserved");
                else if(!std::strcmp(mode,"session_origin_change"))check(frame.origin_generation==2&&!frame.head.flags&&!frame.eyes[1].pose.flags,"origin change invalidates pending samples");
                else check(frame.head.flags&&frame.eyes[0].pose.flags&&frame.eyes[0].pose.position[0]<0&&frame.eyes[1].pose.position[0]>0,"head and stereo tracked poses");
            }
        }
        if(!std::strcmp(mode,"session_stale")) {
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            check(!xr_vulkan_latest_frame(&frame)&&!frame.sequence,"stale sample invalidated during blocked wait");
        }
        const auto first_generation=frame.origin_generation;
        check(xr_vulkan_session_stop(),"bounded stop");
        check(!xr_vulkan_latest_frame(&frame)&&!frame.sequence,"stop clears mailbox");
        if(!std::strcmp(mode,"session_success")) {
            check(xr_vulkan_session_start(instance,physical,device,7,test_queue,30),"restart bounded session");
            bool got_new=false;
            for(int i=0;i<500;++i) {
                if(xr_vulkan_latest_frame(&frame)) {got_new=true;check(frame.origin_generation>first_generation,"restart advances origin generation");}
                if(count(1)>=2)break;
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            check(got_new&&count(1)>=2,"duration closes restarted session");
            check(xr_vulkan_session_stop(),"join completed owner");
        }
        check(count(0)==count(1),"sessions released");check(count(6)==count(7),"partial/complete spaces released");
        check(count(2)==count(3),"every successfully begun frame gets an end attempt");
        check(count(4)==0&&count(5)==0,"zero layers, API ordering, binding and pose timestamps");
        xr_vulkan_reset();check(!xr_vulkan_active(),"reset");
#ifdef _WIN32
        FreeLibrary(library);
#else
        dlclose(library);
#endif
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());xr_vulkan_reset();return 1;}
}