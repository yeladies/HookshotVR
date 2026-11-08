// Isolated OpenXR/Vulkan experiment. Optional synthetic panel; no game or input.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace {
void xr_check(XrResult r, const char* stage) {
    if (XR_FAILED(r)) throw std::runtime_error(std::string(stage) + ": XrResult=" + std::to_string(r));
}
void vk_check(VkResult r, const char* stage) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string(stage) + ": VkResult=" + std::to_string(r));
}
template<class T> T xr_proc(PFN_xrGetInstanceProcAddr get, XrInstance instance, const char* name) {
    PFN_xrVoidFunction p = nullptr;
    xr_check(get(instance, name, &p), name);
    if (!p) throw std::runtime_error(std::string("Missing ") + name);
    return reinterpret_cast<T>(p);
}
template<class T> T vk_proc(PFN_vkGetInstanceProcAddr get, VkInstance instance, const char* name) {
    const auto p = get(instance, name);
    if (!p) throw std::runtime_error(std::string("Missing ") + name);
    return reinterpret_cast<T>(p);
}
struct State {
    HMODULE xr_dll = nullptr, vk_dll = nullptr;
    XrInstance instance = XR_NULL_HANDLE;
    XrSession session = XR_NULL_HANDLE;
    VkInstance vi = VK_NULL_HANDLE;
    VkDevice vd = VK_NULL_HANDLE;
    PFN_xrDestroyInstance destroy_instance = nullptr;
    PFN_xrDestroySession destroy_session = nullptr;
    PFN_vkDestroyInstance destroy_vi = nullptr;
    PFN_vkDestroyDevice destroy_vd = nullptr;
    ~State() {
        // A session shares the Vulkan handles; destroy it first. No GPU work is submitted.
        if (session && destroy_session) destroy_session(session);
        if (vd && destroy_vd) destroy_vd(vd, nullptr);
        if (vi && destroy_vi) destroy_vi(vi, nullptr);
        if (instance && destroy_instance) destroy_instance(instance);
        if (vk_dll) FreeLibrary(vk_dll);
        if (xr_dll) FreeLibrary(xr_dll);
    }
};
#include "xr_quad.h"
bool extension_supported(PFN_xrEnumerateInstanceExtensionProperties enumerate) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        uint32_t n = 0;
        xr_check(enumerate(nullptr, 0, &n, nullptr), "extension count");
        if (n > 4096) throw std::runtime_error("Implausible extension count");
        if (!n) return false;
        std::vector<XrExtensionProperties> all(n, {XR_TYPE_EXTENSION_PROPERTIES});
        const auto r = enumerate(nullptr, n, &n, all.data());
        if (r == XR_ERROR_SIZE_INSUFFICIENT) continue;
        xr_check(r, "extension enumeration");
        if (n > all.size()) throw std::runtime_error("Invalid extension count");
        for (uint32_t i = 0; i < n; ++i)
            if (!std::strcmp(all[i].extensionName, XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) return true;
        return false;
    }
    throw std::runtime_error("Unstable extension list");
}
int run(const std::filesystem::path& loader, bool create_session, int quad_seconds) {
    State s;
    s.xr_dll = LoadLibraryExW(loader.c_str(), nullptr,
                            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!s.xr_dll) throw std::runtime_error("OpenXR loader load failed, Win32=" + std::to_string(GetLastError()));
    const auto get = reinterpret_cast<PFN_xrGetInstanceProcAddr>(GetProcAddress(s.xr_dll, "xrGetInstanceProcAddr"));
    if (!get) throw std::runtime_error("Loader has no xrGetInstanceProcAddr");
    const auto enumerate = xr_proc<PFN_xrEnumerateInstanceExtensionProperties>(get, XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties");
    if (!extension_supported(enumerate)) throw std::runtime_error("XR_KHR_vulkan_enable2 is absent");
    const char* ext = XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME;
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    std::snprintf(ci.applicationInfo.applicationName, sizeof(ci.applicationInfo.applicationName), "Hookshot isolated XR smoke");
    ci.applicationInfo.applicationVersion = 1;
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    ci.enabledExtensionCount = 1;
    ci.enabledExtensionNames = &ext;
    const auto create = xr_proc<PFN_xrCreateInstance>(get, XR_NULL_HANDLE, "xrCreateInstance");
    xr_check(create(&ci, &s.instance), "xrCreateInstance");
    s.destroy_instance = xr_proc<PFN_xrDestroyInstance>(get, s.instance, "xrDestroyInstance");
    XrInstanceProperties ip{XR_TYPE_INSTANCE_PROPERTIES};
    xr_check(xr_proc<PFN_xrGetInstanceProperties>(get, s.instance, "xrGetInstanceProperties")(s.instance, &ip), "xrGetInstanceProperties");
    std::printf("runtime=%s version=%u.%u.%u\n", ip.runtimeName,
                XR_VERSION_MAJOR(ip.runtimeVersion), XR_VERSION_MINOR(ip.runtimeVersion), XR_VERSION_PATCH(ip.runtimeVersion));
    XrSystemGetInfo gi{XR_TYPE_SYSTEM_GET_INFO};
    gi.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    const auto system_result = xr_proc<PFN_xrGetSystem>(get, s.instance, "xrGetSystem")(s.instance, &gi, &system);
    if (system_result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
        std::puts("status=hmd_unavailable; connect the headset to the selected PC runtime and retry");
        return 3;
    }
    xr_check(system_result, "xrGetSystem");
    XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
    xr_check(xr_proc<PFN_xrGetVulkanGraphicsRequirements2KHR>(get, s.instance, "xrGetVulkanGraphicsRequirements2KHR")(
                 s.instance, system, &req), "xrGetVulkanGraphicsRequirements2KHR");
    std::printf("vulkan_runtime_min=%u.%u tested_max=%u.%u\n",
                XR_VERSION_MAJOR(req.minApiVersionSupported), XR_VERSION_MINOR(req.minApiVersionSupported),
                XR_VERSION_MAJOR(req.maxApiVersionSupported), XR_VERSION_MINOR(req.maxApiVersionSupported));
    // Deliberately conservative: this experiment does not try beyond the runtime's tested maximum.
    const XrVersion needed = XR_MAKE_VERSION(1, 3, 0);
    if (needed < req.minApiVersionSupported || needed > req.maxApiVersionSupported)
        throw std::runtime_error("Vulkan 1.3 lies outside this experiment's accepted runtime range");
    if (!create_session) {
        std::puts("status=requirements_queried; session_created=false; frame_submitted=false");
        return 0;
    }
    s.vk_dll = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!s.vk_dll) throw std::runtime_error("System Vulkan loader load failed");
    const auto vget = reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(s.vk_dll, "vkGetInstanceProcAddr"));
    if (!vget) throw std::runtime_error("No vkGetInstanceProcAddr");
    VkApplicationInfo ai{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    ai.pApplicationName = "Hookshot XR smoke";
    ai.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo vci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vci.pApplicationInfo = &ai;
    XrVulkanInstanceCreateInfoKHR xvi{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xvi.systemId = system;
    xvi.pfnGetInstanceProcAddr = vget;
    xvi.vulkanCreateInfo = &vci;
    VkResult vr = VK_ERROR_INITIALIZATION_FAILED;
    xr_check(xr_proc<PFN_xrCreateVulkanInstanceKHR>(get, s.instance, "xrCreateVulkanInstanceKHR")(
                 s.instance, &xvi, &s.vi, &vr), "xrCreateVulkanInstanceKHR");
    vk_check(vr, "wrapped vkCreateInstance");
    s.destroy_vi = vk_proc<PFN_vkDestroyInstance>(vget, s.vi, "vkDestroyInstance");
    XrVulkanGraphicsDeviceGetInfoKHR dgi{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    dgi.systemId = system;
    dgi.vulkanInstance = s.vi;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    xr_check(xr_proc<PFN_xrGetVulkanGraphicsDevice2KHR>(get, s.instance, "xrGetVulkanGraphicsDevice2KHR")(
                 s.instance, &dgi, &physical), "xrGetVulkanGraphicsDevice2KHR");
    VkPhysicalDeviceProperties props{};
    vk_proc<PFN_vkGetPhysicalDeviceProperties>(vget, s.vi, "vkGetPhysicalDeviceProperties")(physical, &props);
    std::printf("runtime_gpu=%s vulkan=%u.%u.%u\n", props.deviceName,
                VK_API_VERSION_MAJOR(props.apiVersion), VK_API_VERSION_MINOR(props.apiVersion), VK_API_VERSION_PATCH(props.apiVersion));
    if (props.apiVersion < VK_API_VERSION_1_3) throw std::runtime_error("Runtime GPU lacks Vulkan 1.3");
    const auto queues = vk_proc<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(vget, s.vi, "vkGetPhysicalDeviceQueueFamilyProperties");
    uint32_t n = 0;
    queues(physical, &n, nullptr);
    std::vector<VkQueueFamilyProperties> q(n);
    queues(physical, &n, q.data());
    uint32_t family = 0;
    while (family < n && (!(q[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) || !q[family].queueCount)) ++family;
    if (family == n) throw std::runtime_error("No graphics queue on runtime GPU");
    const float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qi;
    XrVulkanDeviceCreateInfoKHR xdi{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xdi.systemId = system;
    xdi.pfnGetInstanceProcAddr = vget;
    xdi.vulkanPhysicalDevice = physical;
    xdi.vulkanCreateInfo = &dci;
    vr = VK_ERROR_INITIALIZATION_FAILED;
    xr_check(xr_proc<PFN_xrCreateVulkanDeviceKHR>(get, s.instance, "xrCreateVulkanDeviceKHR")(
                 s.instance, &xdi, &s.vd, &vr), "xrCreateVulkanDeviceKHR");
    vk_check(vr, "wrapped vkCreateDevice");
    const auto dget = vk_proc<PFN_vkGetDeviceProcAddr>(vget, s.vi, "vkGetDeviceProcAddr");
    s.destroy_vd = reinterpret_cast<PFN_vkDestroyDevice>(dget(s.vd, "vkDestroyDevice"));
    if (!s.destroy_vd) throw std::runtime_error("No vkDestroyDevice");
    XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
    binding.instance = s.vi;
    binding.physicalDevice = physical;
    binding.device = s.vd;
    binding.queueFamilyIndex = family;
    binding.queueIndex = 0;
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &binding;
    sci.systemId = system;
    s.destroy_session = xr_proc<PFN_xrDestroySession>(get, s.instance, "xrDestroySession");
    xr_check(xr_proc<PFN_xrCreateSession>(get, s.instance, "xrCreateSession")(s.instance, &sci, &s.session), "xrCreateSession");
    std::printf("queue_family=%u queue_index=0\n", family);
    if (quad_seconds) return quad_test(s, get, vget, dget, system, family, quad_seconds);
    std::puts("status=session_created; frame_submitted=false; game_feature_compatibility=not_tested");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    std::filesystem::path loader;
    bool session = false;
    int quad_seconds = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--help")) {
            std::puts("hookshot_xr_smoke --loader ABSOLUTE_DLL_PATH [--session] [--quad-seconds 1..60]\n"
                      "Default: instance, HMD system and Vulkan requirements only.\n"
                      "--session: also create/destroy a Vulkan device and idle XR session.\n"
                      "--quad-seconds: brief dim blue panel; no game or input.\n"
                      "Exit 0=completed, 1=runtime error, 2=usage, 3=HMD unavailable, 4=GPU timeout.");
            return 0;
        }
        if (!std::strcmp(argv[i], "--quad-seconds") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos || value.size() > 2) {
                std::fprintf(stderr, "--quad-seconds must be 1..60\n"); return 2;
            }
            quad_seconds = std::stoi(value);
            if (quad_seconds < 1 || quad_seconds > 60) { std::fprintf(stderr, "--quad-seconds must be 1..60\n"); return 2; }
            session = true;
        }
        else if (!std::strcmp(argv[i], "--session")) session = true;
        else if (!std::strcmp(argv[i], "--loader") && i + 1 < argc) loader = std::filesystem::u8path(argv[++i]);
        else { std::fprintf(stderr, "Invalid argument; use --help\n"); return 2; }
    }
    if (loader.empty() || !loader.is_absolute()) {
        std::fprintf(stderr, "--loader requires an absolute trusted x64 OpenXR loader path\n");
        return 2;
    }
    try { return run(loader, session, quad_seconds); }
    catch (const std::exception& e) { std::fprintf(stderr, "status=failed error=%s\n", e.what()); return 1; }
}
