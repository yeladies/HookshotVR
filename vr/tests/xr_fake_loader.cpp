// SPDX-License-Identifier: GPL-3.0-or-later
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdlib>
#include <cstring>
#include <cstdint>
namespace {
bool mode(const char* value) { const char* s=std::getenv("XR_FAKE_MODE");return s&&!std::strcmp(s,value); }
XrResult XRAPI_CALL extensions(const char*,uint32_t cap,uint32_t* count,XrExtensionProperties* props) {
    *count=mode("no_extension")?0:1;
    if(cap&&*count) std::strcpy(props[0].extensionName,XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
    return XR_SUCCESS;
}
XrResult XRAPI_CALL create(const XrInstanceCreateInfo*,XrInstance* out) {
    *out=reinterpret_cast<XrInstance>(uintptr_t(1));return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy(XrInstance) { return XR_SUCCESS; }
XrResult XRAPI_CALL fake_system(XrInstance,const XrSystemGetInfo*,XrSystemId* out) {
    if(mode("no_headset"))return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    *out=1;return XR_SUCCESS;
}
XrResult XRAPI_CALL requirements(XrInstance,XrSystemId,XrGraphicsRequirementsVulkanKHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,0,0);
    out->maxApiVersionSupported=mode("range")?XR_MAKE_VERSION(1,2,0):XR_MAKE_VERSION(1,3,0);
    return XR_SUCCESS;
}
XrResult XRAPI_CALL instance(XrInstance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    if(!info->vulkanCreateInfo||info->vulkanCreateInfo->pApplicationInfo->apiVersion!=VK_API_VERSION_1_3)return XR_ERROR_VALIDATION_FAILURE;
    *out=reinterpret_cast<VkInstance>(uintptr_t(2));
    *result=mode("vk_instance_error")?VK_ERROR_INITIALIZATION_FAILED:VK_SUCCESS;
    return mode("xr_instance_error")?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL physical(XrInstance,const XrVulkanGraphicsDeviceGetInfoKHR*,VkPhysicalDevice* out) {
    *out=reinterpret_cast<VkPhysicalDevice>(uintptr_t(3));return XR_SUCCESS;
}
XrResult XRAPI_CALL device(XrInstance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    if(info->vulkanPhysicalDevice!=reinterpret_cast<VkPhysicalDevice>(uintptr_t(3)))return XR_ERROR_VALIDATION_FAILURE;
    *out=reinterpret_cast<VkDevice>(uintptr_t(4));
    *result=mode("vk_device_error")?VK_ERROR_FEATURE_NOT_PRESENT:VK_SUCCESS;
    return mode("xr_device_error")?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
}
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
extern "C" EXPORT XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;
    if(mode("missing_function")&&!std::strcmp(name,"xrGetSystem"))return XR_ERROR_FUNCTION_UNSUPPORTED;
#define MAP(n,f) if(!std::strcmp(name,n)){*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    MAP("xrEnumerateInstanceExtensionProperties",extensions)
    MAP("xrCreateInstance",create) MAP("xrDestroyInstance",destroy) MAP("xrGetSystem",fake_system)
    MAP("xrGetVulkanGraphicsRequirements2KHR",requirements)
    MAP("xrCreateVulkanInstanceKHR",instance) MAP("xrGetVulkanGraphicsDevice2KHR",physical)
    MAP("xrCreateVulkanDeviceKHR",device)
#undef MAP
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}