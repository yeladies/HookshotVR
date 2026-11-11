// SPDX-License-Identifier: GPL-3.0-or-later
#include "probe.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace hookshot::vr;
namespace {
struct Fake {
    bool extension = true, stereo = true, position = true, grow = false, invalid_eye = false;
    XrResult create_result = XR_SUCCESS, system_result = XR_SUCCESS, views_result = XR_SUCCESS;
    int creates = 0, destroys = 0, fills = 0;
} f;
void require(bool good, const char* message) { if (!good) throw std::runtime_error(message); }
XrResult XRAPI_PTR extensions(const char*, uint32_t cap, uint32_t* n, XrExtensionProperties* p) {
    *n = f.extension ? (f.grow && f.fills ? 2 : 1) : 0;
    if (cap && f.grow && f.fills++ == 0) { *n = 2; return XR_ERROR_SIZE_INSUFFICIENT; }
    if (cap) {
        require(p[0].type == XR_TYPE_EXTENSION_PROPERTIES, "extension type initialized");
        std::strcpy(p[0].extensionName, "XR_KHR_vulkan_enable2");
        if (*n == 2) std::strcpy(p[1].extensionName, "XR_FAKE_extra");
    }
    return XR_SUCCESS;
}
XrResult XRAPI_PTR create(const XrInstanceCreateInfo* info, XrInstance* instance) {
    ++f.creates;
    require(info->enabledExtensionCount == 1 && !std::strcmp(info->enabledExtensionNames[0], "XR_KHR_vulkan_enable2"),
            "enable Vulkan extension");
    require(info->applicationInfo.apiVersion == XR_MAKE_VERSION(1, 0, 0), "compatible API requested");
    if (XR_SUCCEEDED(f.create_result)) *instance = reinterpret_cast<XrInstance>(uintptr_t(1));
    return f.create_result;
}
XrResult XRAPI_PTR destroy(XrInstance) { ++f.destroys; return XR_SUCCESS; }
XrResult XRAPI_PTR properties(XrInstance, XrInstanceProperties* p) {
    require(p->type == XR_TYPE_INSTANCE_PROPERTIES, "properties initialized");
    std::strcpy(p->runtimeName, "Fake \"runtime\"\n");
    p->runtimeVersion = XR_MAKE_VERSION(1, 2, 3);
    return XR_SUCCESS;
}
XrResult XRAPI_PTR system_id(XrInstance, const XrSystemGetInfo*, XrSystemId* id) { *id = 1; return f.system_result; }
XrResult XRAPI_PTR system_properties(XrInstance, XrSystemId, XrSystemProperties* p) {
    require(p->type == XR_TYPE_SYSTEM_PROPERTIES, "system properties initialized");
    std::strcpy(p->systemName, "Fake headset");
    p->trackingProperties.orientationTracking = XR_TRUE;
    p->trackingProperties.positionTracking = f.position ? XR_TRUE : XR_FALSE;
    return XR_SUCCESS;
}
XrResult XRAPI_PTR types(XrInstance, XrSystemId, uint32_t cap, uint32_t* n, XrViewConfigurationType* p) {
    *n = 1;
    if (cap) *p = f.stereo ? XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO : XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO;
    return XR_SUCCESS;
}
XrResult XRAPI_PTR views(XrInstance, XrSystemId, XrViewConfigurationType, uint32_t cap, uint32_t* n, XrViewConfigurationView* p) {
    *n = 2;
    if (XR_FAILED(f.views_result)) return f.views_result;
    if (cap) for (int i = 0; i < 2; ++i) {
        require(p[i].type == XR_TYPE_VIEW_CONFIGURATION_VIEW, "view type initialized");
        p[i].recommendedImageRectWidth = f.invalid_eye ? 0 : 1200;
        p[i].recommendedImageRectHeight = 1300;
        p[i].recommendedSwapchainSampleCount = 1;
    }
    return XR_SUCCESS;
}
const Dispatch api{extensions, create, destroy, properties, system_id, system_properties, types, views};
Report run() { return probe(api); }
} // namespace

int main() {
    try {
        f = {};
        auto r = run();
        require(r.status == Status::ready && r.eyes.size() == 2, "stereo/6DoF capabilities recognized");
        require(f.creates == 1 && f.destroys == 1, "successful instance cleaned up");
        const auto json = to_json(r);
        require(json.find("Fake \\\"runtime\\\"\\u000a") != std::string::npos, "JSON escapes runtime strings");
        require(json.find("\"session_tested\": false") != std::string::npos, "capability discovery does not claim session");
        f = {}; f.extension = false;
        require(run().status == Status::unsupported && f.creates == 0, "missing Vulkan prevents instance creation");
        f = {}; f.system_result = XR_ERROR_FORM_FACTOR_UNAVAILABLE;
        require(run().status == Status::hmd_unavailable && f.destroys == 1, "disconnected HMD cleaned up and classified");
        f = {}; f.create_result = XR_ERROR_RUNTIME_FAILURE;
        require(run().status == Status::runtime_error && f.destroys == 0, "failed creation does not destroy null handle");
        f = {}; f.stereo = false;
        require(run().status == Status::unsupported && f.destroys == 1, "mono rejected");
        f = {}; f.position = false;
        require(run().status == Status::unsupported, "3DoF rejected");
        f = {}; f.invalid_eye = true;
        require(run().status == Status::unsupported, "invalid eye dimensions rejected");
        f = {}; f.views_result = XR_ERROR_RUNTIME_FAILURE;
        require(run().status == Status::runtime_error && f.destroys == 1, "enumeration failure cleaned up");
        f = {}; f.grow = true;
        r = run();
        require(r.status == Status::ready && r.extensions.size() == 2, "extension count growth retried");
        require(exit_code(Status::ready) == 0 && exit_code(Status::runtime_error) == 2 &&
                exit_code(Status::hmd_unavailable) == 3 && exit_code(Status::unsupported) == 4, "exit codes stable");
        std::cout << "10 probe contract checks passed\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
