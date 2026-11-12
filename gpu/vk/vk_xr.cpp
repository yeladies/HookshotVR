// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include "xr_preview_feed.h"
namespace mkwvk { void logf(const char*, ...); }
#ifdef HOOKSHOT_OPENXR
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace mkwvk {
namespace {
struct Bridge {
    void* library = nullptr;
    PFN_xrGetInstanceProcAddr get = nullptr;
    PFN_xrDestroyInstance destroy = nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    bool ready = false;
    std::thread worker;
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> origin_epoch{0};
    std::atomic<bool> preview_accepting{false},readback_only{false};
    PreviewFeed preview_feed;
    PFN_vkGetInstanceProcAddr vulkan_get=nullptr;
    bool retained_gpu_work=false;
    std::mutex worker_mutex, mailbox_mutex;
    std::condition_variable worker_cv;
    bool initialized=false, start_ok=false, done=true;
    XrFrameSnapshot latest{};
    template<class T> bool function(const char* name, T& out) {
        PFN_xrVoidFunction raw = nullptr;
        const XrResult result = get(instance, name, &raw);
        out = reinterpret_cast<T>(raw);
        if (XR_FAILED(result) || !out) { logf("xr: missing %s (%d)", name, int(result)); return false; }
        return true;
    }

    void reset() {
        if (!xr_vulkan_session_stop() || retained_gpu_work) return;
        ready = false;
        if (instance && destroy) destroy(instance);
        instance = XR_NULL_HANDLE; system = XR_NULL_SYSTEM_ID; destroy = nullptr; get = nullptr;
#ifdef _WIN32
        if (library) FreeLibrary(static_cast<HMODULE>(library));
#else
        if (library) dlclose(library);
#endif
        library = nullptr;
    }
};
// The host keeps its renderer DLL loaded for the process lifetime. Explicit reset/close joins
// the owner thread; avoid joining a thread from CRT destructors under Windows loader lock.
Bridge& bridge=*new Bridge;
bool ok(XrResult result, const char* operation) {
    if (XR_SUCCEEDED(result)) return true;
    logf("xr: %s failed (%d); XR adapter operation failed", operation, int(result));
    return false;
}
}
} // namespace mkwvk
#include "vk_xr_preview.h"
#include "vk_xr_session.h"
namespace mkwvk {
bool xr_vulkan_session_stop() {
    bridge.stop.store(true);
    bridge.preview_accepting.store(false);bridge.preview_feed.clear();
    { std::lock_guard lock(bridge.mailbox_mutex);bridge.latest={}; }
    if(bridge.worker.joinable()) {
        bridge.stop.store(true);
        std::unique_lock lock(bridge.worker_mutex);
        if(!bridge.worker_cv.wait_for(lock,std::chrono::seconds(5),[]{return bridge.done;})) {
            logf("xr: session owner did not stop within 5 s; retaining runtime resources until process exit");
            return false; // Never unload code/handles still in use by the owner.
        }
        lock.unlock();bridge.worker.join();
    }
    std::lock_guard lock(bridge.mailbox_mutex);bridge.latest={};
    return true;
}
bool xr_vulkan_latest_frame(XrFrameSnapshot* out) {
    if(!out)return false;
    std::lock_guard lock(bridge.mailbox_mutex);
    *out=bridge.latest;
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(bridge.stop.load()||!out->sequence||!out->running||now-out->observed_at_monotonic_ns>250000000) {
        *out={};return false;
    }
    return true;
}
bool xr_vulkan_session_start(VkInstance instance,VkPhysicalDevice physical,VkDevice device,uint32_t queue_family,std::mutex& queue_mutex,uint32_t duration_ms,bool visual_preview) {
    if(!bridge.ready||duration_ms==0||duration_ms>180000||!xr_vulkan_session_stop())return false;
    bridge.stop.store(false);
    { std::lock_guard lock(bridge.worker_mutex);bridge.initialized=false;bridge.start_ok=false;bridge.done=false; }
    try {
        bridge.worker=std::thread([=,&queue_mutex] {
            SessionProbe probe(bridge,queue_mutex,visual_preview);
            try {
                const bool started=probe.initialize(instance,physical,device,queue_family);
                { std::lock_guard lock(bridge.worker_mutex);bridge.start_ok=started;bridge.initialized=true; }
                bridge.worker_cv.notify_all();
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(duration_ms);
                while(started&&!bridge.stop.load()&&std::chrono::steady_clock::now()<deadline) {
                    if(!probe.tick())break;
                    { std::lock_guard lock(bridge.mailbox_mutex);bridge.latest=bridge.stop.load()?XrFrameSnapshot{}:probe.snapshot; }
                    if(!probe.running)std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            } catch(const std::exception& e) { logf("xr: session owner exception: %s",e.what()); }
            probe.close();
            { std::lock_guard lock(bridge.mailbox_mutex);bridge.latest={}; }
            { std::lock_guard lock(bridge.worker_mutex);bridge.initialized=true;bridge.done=true; }
            bridge.worker_cv.notify_all();
        });
    } catch(const std::exception& e) {
        logf("xr: cannot start session owner: %s",e.what());
        std::lock_guard lock(bridge.worker_mutex);bridge.done=true;return false;
    }
    std::unique_lock lock(bridge.worker_mutex);
    if(!bridge.worker_cv.wait_for(lock,std::chrono::seconds(10),[]{return bridge.initialized;})) {
        bridge.stop.store(true);logf("xr: session startup timed out");return false;
    }
    return bridge.start_ok;
}
bool xr_preview_wants_frame() {return (bridge.preview_accepting.load()||bridge.readback_only.load())&&!bridge.stop.load();}
void xr_preview_publish(std::shared_ptr<const std::vector<uint8_t>> pixels,uint32_t width,uint32_t height,int64_t present) {
    if(!xr_preview_wants_frame())return;
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if(bridge.preview_feed.publish(std::move(pixels),width,height,present,now)&&bridge.readback_only.load()) {
        const auto image=bridge.preview_feed.latest(now);
        if(image.serial==1||image.serial%60==0)logf("xr-preview: readback frames %llu, %ux%u RGBA, no XR session",
            (unsigned long long)image.serial,width,height);
    }
}
void xr_vulkan_reset() { bridge.reset(); }
bool xr_vulkan_active() { return bridge.ready; }
bool xr_vulkan_prepare(bool requested) {
    bridge.reset();
    if (bridge.worker.joinable() || bridge.retained_gpu_work) return false;
    const char* readback=std::getenv("HOOKSHOT_VR_PREVIEW_READBACK_TEST");
    bridge.readback_only.store(readback&&std::strcmp(readback,"1")==0);
    if(bridge.readback_only.load()) {
        if(requested){logf("xr: readback-only test cannot open an XR session");return false;}
        bridge.stop.store(false);
    }
    if (!requested) return true;
    const char* path = std::getenv("HOOKSHOT_OPENXR_LOADER");
    if (!path || !*path || !std::filesystem::u8path(path).is_absolute()) {
        logf("xr: HOOKSHOT_OPENXR_LOADER must name an absolute local official loader path"); return false;
    }
#ifdef _WIN32
    bridge.library = LoadLibraryExW(std::filesystem::u8path(path).c_str(), nullptr,
                                   LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (bridge.library) bridge.get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(
        GetProcAddress(static_cast<HMODULE>(bridge.library), "xrGetInstanceProcAddr"));
#else
    bridge.library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (bridge.library) bridge.get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(dlsym(bridge.library,"xrGetInstanceProcAddr"));
#endif
    if (!bridge.get) { logf("xr: official loader unavailable"); bridge.reset(); return false; }
    PFN_xrEnumerateInstanceExtensionProperties enumerate = nullptr;
    PFN_xrCreateInstance create = nullptr;
    if (!bridge.function("xrEnumerateInstanceExtensionProperties", enumerate) || !bridge.function("xrCreateInstance", create)) return false;
    uint32_t count = 0;
    if (!ok(enumerate(nullptr,0,&count,nullptr),"extension count")) return false;
    std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
    if (!ok(enumerate(nullptr,count,&count,extensions.data()),"extensions")) return false;
    bool found = false;
    for (const auto& extension : extensions) found |= !std::strcmp(extension.extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
    if (!found) { logf("xr: runtime lacks XR_KHR_vulkan_enable2"); return false; }
    XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(info.applicationInfo.applicationName,"HookshotVR negotiation");
    info.applicationInfo.apiVersion = XR_MAKE_VERSION(1,0,0);
    const char* extension = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    info.enabledExtensionCount = 1; info.enabledExtensionNames = &extension;
    if (!ok(create(&info,&bridge.instance),"create instance")) return false;
    if (!bridge.function("xrDestroyInstance",bridge.destroy)) return false;
    PFN_xrGetSystem get_system = nullptr;
    PFN_xrGetVulkanGraphicsRequirements2KHR requirements = nullptr;
    if (!bridge.function("xrGetSystem",get_system) || !bridge.function("xrGetVulkanGraphicsRequirements2KHR",requirements)) return false;
    XrSystemGetInfo system_info{XR_TYPE_SYSTEM_GET_INFO}; system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    if (!ok(get_system(bridge.instance,&system_info,&bridge.system),"get HMD")) return false;
    XrGraphicsRequirementsVulkan2KHR limits{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    if (!ok(requirements(bridge.instance,bridge.system,&limits),"Vulkan requirements")) return false;
    const XrVersion wanted = XR_MAKE_VERSION(1,3,0);
    if (wanted < limits.minApiVersionSupported || wanted > limits.maxApiVersionSupported) {
        logf("xr: runtime Vulkan range does not include Hookshot Vulkan 1.3"); return false;
    }
    bridge.ready = true;
    logf("xr: Vulkan enable2 negotiation ready; no XR session or stereo output yet");
    return true;
}
bool xr_vulkan_instance(PFN_vkGetInstanceProcAddr get, const VkInstanceCreateInfo& info, VkInstance* out) {
    bridge.vulkan_get=get;
    PFN_xrCreateVulkanInstanceKHR create = nullptr;
    if (!bridge.ready || !bridge.function("xrCreateVulkanInstanceKHR",create)) return false;
    XrVulkanInstanceCreateInfoKHR request{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    request.systemId=bridge.system; request.pfnGetInstanceProcAddr=get; request.vulkanCreateInfo=&info;
    VkResult result=VK_ERROR_INITIALIZATION_FAILED;
    return ok(create(bridge.instance,&request,out,&result),"create Vulkan instance") &&
           (result==VK_SUCCESS || (logf("xr: Vulkan instance result %d",int(result)),false));
}
bool xr_vulkan_physical(VkInstance instance, VkPhysicalDevice* out) {
    PFN_xrGetVulkanGraphicsDevice2KHR get = nullptr;
    if (!bridge.ready || !bridge.function("xrGetVulkanGraphicsDevice2KHR",get)) return false;
    XrVulkanGraphicsDeviceGetInfoKHR request{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    request.systemId=bridge.system; request.vulkanInstance=instance;
    return ok(get(bridge.instance,&request,out),"select Vulkan physical device") && *out!=VK_NULL_HANDLE;
}
bool xr_vulkan_device(PFN_vkGetInstanceProcAddr get, VkPhysicalDevice physical, const VkDeviceCreateInfo& info, VkDevice* out) {
    PFN_xrCreateVulkanDeviceKHR create = nullptr;
    if (!bridge.ready || !bridge.function("xrCreateVulkanDeviceKHR",create)) return false;
    XrVulkanDeviceCreateInfoKHR request{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    request.systemId=bridge.system; request.pfnGetInstanceProcAddr=get;
    request.vulkanPhysicalDevice=physical; request.vulkanCreateInfo=&info;
    VkResult result=VK_ERROR_INITIALIZATION_FAILED;
    const bool success=ok(create(bridge.instance,&request,out,&result),"create Vulkan device") && result==VK_SUCCESS;
    if (!success) logf("xr: Vulkan device result %d",int(result));
    else logf("xr: renderer Vulkan device negotiated; game still uses flat presentation");
    return success;
}
}
#else
namespace mkwvk {
bool xr_vulkan_prepare(bool requested) {
    if (requested) logf("xr: renderer was built without HOOKSHOT_OPENXR_SDK; cannot enable XR negotiation");
    return !requested;
}
bool xr_vulkan_active() { return false; }
bool xr_preview_wants_frame() {return false;}
void xr_preview_publish(std::shared_ptr<const std::vector<uint8_t>>,uint32_t,uint32_t,int64_t) {}
bool xr_vulkan_session_start(VkInstance,VkPhysicalDevice,VkDevice,uint32_t,std::mutex&,uint32_t,bool) { return false; }
bool xr_vulkan_session_stop() { return true; }
bool xr_vulkan_latest_frame(XrFrameSnapshot* out) { if(out)*out={};return false; }
void xr_vulkan_reset() {}
bool xr_vulkan_instance(PFN_vkGetInstanceProcAddr, const VkInstanceCreateInfo&, VkInstance*) { return false; }
bool xr_vulkan_physical(VkInstance, VkPhysicalDevice*) { return false; }
bool xr_vulkan_device(PFN_vkGetInstanceProcAddr, VkPhysicalDevice, const VkDeviceCreateInfo&, VkDevice*) { return false; }
}
#endif