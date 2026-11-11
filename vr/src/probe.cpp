// SPDX-License-Identifier: GPL-3.0-or-later
#include "probe.h"
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace hookshot::vr {
namespace {
constexpr const char* vulkan_extension = "XR_KHR_vulkan_enable2";
// Runtime enumeration can grow between count and fill. Retry with a bound.
template<class T, class Call, class Init>
XrResult enumerate(std::vector<T>& values, Call call, Init init) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        uint32_t count = 0;
        XrResult r = call(0, &count, nullptr);
        if (XR_FAILED(r)) return r;
        if (count > 65536) return XR_ERROR_RUNTIME_FAILURE;
        values.assign(count, T{});
        for (auto& value : values) init(value);
        if (!count) return XR_SUCCESS;
        uint32_t written = count;
        r = call(count, &written, values.data());
        if (r == XR_ERROR_SIZE_INSUFFICIENT) continue;
        if (XR_FAILED(r)) return r;
        if (written > count) return XR_ERROR_RUNTIME_FAILURE;
        values.resize(written);
        return r;
    }
    return XR_ERROR_SIZE_INSUFFICIENT;
}
struct Instance {
    XrInstance handle = XR_NULL_HANDLE;
    PFN_xrDestroyInstance destroy;
    ~Instance() { if (handle != XR_NULL_HANDLE) destroy(handle); }
};
std::string quote(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 0x20) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else out << c;
    }
    out << '"';
    return out.str();
}
const char* status_name(Status s) {
    switch (s) {
    case Status::ready: return "capabilities_ready";
    case Status::hmd_unavailable: return "hmd_unavailable";
    case Status::unsupported: return "unsupported";
    default: return "runtime_error";
    }
}
} // namespace

Report probe(const Dispatch& api) {
    Report out;
    auto check = [&](XrResult r, const char* stage) {
        out.stage = stage;
        out.result = r;
        if (XR_FAILED(r)) {
            out.status = r == XR_ERROR_FORM_FACTOR_UNAVAILABLE ? Status::hmd_unavailable : Status::runtime_error;
            return false;
        }
        return true;
    };
    std::vector<XrExtensionProperties> extensions;
    if (!check(enumerate(extensions, [&](uint32_t n, uint32_t* count, XrExtensionProperties* p) {
        return api.enumerate_extensions(nullptr, n, count, p);
    }, [](auto& p) { p.type = XR_TYPE_EXTENSION_PROPERTIES; }), "enumerate_extensions")) return out;
    for (const auto& p : extensions) {
        out.extensions.emplace_back(p.extensionName);
        if (!std::strcmp(p.extensionName, vulkan_extension)) out.vulkan_enable2 = true;
    }
    std::sort(out.extensions.begin(), out.extensions.end());
    if (!out.vulkan_enable2) {
        out.status = Status::unsupported;
        out.stage = "vulkan_enable2_missing";
        out.result = XR_ERROR_EXTENSION_NOT_PRESENT;
        return out;
    }
    const char* enabled[] = {vulkan_extension};
    XrInstanceCreateInfo info{};
    info.type = XR_TYPE_INSTANCE_CREATE_INFO;
    std::strcpy(info.applicationInfo.applicationName, "HookshotVR Probe");
    std::strcpy(info.applicationInfo.engineName, "HookshotVR");
    info.applicationInfo.applicationVersion = 1;
    info.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);
    info.enabledExtensionCount = 1;
    info.enabledExtensionNames = enabled;
    Instance instance{XR_NULL_HANDLE, api.destroy_instance};
    if (!check(api.create_instance(&info, &instance.handle), "create_instance")) return out;
    XrInstanceProperties properties{};
    properties.type = XR_TYPE_INSTANCE_PROPERTIES;
    if (!check(api.instance_properties(instance.handle, &properties), "instance_properties")) return out;
    out.runtime = properties.runtimeName;
    out.runtime_version = properties.runtimeVersion;
    XrSystemGetInfo system_info{};
    system_info.type = XR_TYPE_SYSTEM_GET_INFO;
    system_info.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId system = XR_NULL_SYSTEM_ID;
    if (!check(api.get_system(instance.handle, &system_info, &system), "get_system")) return out;
    XrSystemProperties system_properties{};
    system_properties.type = XR_TYPE_SYSTEM_PROPERTIES;
    if (!check(api.system_properties(instance.handle, system, &system_properties), "system_properties")) return out;
    out.system = system_properties.systemName;
    out.orientation_tracking = system_properties.trackingProperties.orientationTracking != XR_FALSE;
    out.position_tracking = system_properties.trackingProperties.positionTracking != XR_FALSE;
    std::vector<XrViewConfigurationType> types;
    if (!check(enumerate(types, [&](uint32_t n, uint32_t* count, XrViewConfigurationType* p) {
        return api.enumerate_view_types(instance.handle, system, n, count, p);
    }, [](auto&) {}), "enumerate_view_types")) return out;
    if (std::find(types.begin(), types.end(), XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) == types.end()) {
        out.status = Status::unsupported;
        out.stage = "primary_stereo_missing";
        out.result = XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED;
        return out;
    }
    std::vector<XrViewConfigurationView> views;
    if (!check(enumerate(views, [&](uint32_t n, uint32_t* count, XrViewConfigurationView* p) {
        return api.enumerate_views(instance.handle, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, count, p);
    }, [](auto& p) { p.type = XR_TYPE_VIEW_CONFIGURATION_VIEW; }), "enumerate_views")) return out;
    for (const auto& v : views) out.eyes.push_back({v.recommendedImageRectWidth, v.recommendedImageRectHeight,
                                                  v.recommendedSwapchainSampleCount});
    const bool valid_eyes = views.size() == 2 && std::all_of(out.eyes.begin(), out.eyes.end(), [](auto v) {
        return v.width > 0 && v.height > 0 && v.samples > 0;
    });
    out.status = valid_eyes && out.position_tracking && out.orientation_tracking ? Status::ready : Status::unsupported;
    out.stage = out.status == Status::ready ? "capability_check_complete" : "stereo_or_6dof_missing";
    // A successful capability check does not establish a session, poses or rendered stereo.
    return out;
}
int exit_code(Status status) {
    switch (status) {
    case Status::ready: return 0;
    case Status::hmd_unavailable: return 3;
    case Status::unsupported: return 4;
    default: return 2;
    }
}
std::string to_json(const Report& r) {
    std::ostringstream out;
    out << "{\n  \"schema_version\": 1,\n  \"status\": " << quote(status_name(r.status))
        << ",\n  \"stage\": " << quote(r.stage) << ",\n  \"xr_result\": " << int(r.result)
        << ",\n  \"runtime\": " << quote(r.runtime) << ",\n  \"runtime_version\": "
        << quote(std::to_string(XR_VERSION_MAJOR(r.runtime_version)) + "." + std::to_string(XR_VERSION_MINOR(r.runtime_version))
                 + "." + std::to_string(XR_VERSION_PATCH(r.runtime_version)))
        << ",\n  \"system\": " << quote(r.system)
        << ",\n  \"vulkan_enable2\": " << (r.vulkan_enable2 ? "true" : "false")
        << ",\n  \"orientation_tracking\": " << (r.orientation_tracking ? "true" : "false")
        << ",\n  \"position_tracking\": " << (r.position_tracking ? "true" : "false")
        << ",\n  \"session_tested\": false,\n  \"rendering_tested\": false,\n  \"eyes\": [";
    for (size_t i = 0; i < r.eyes.size(); ++i) {
        const auto& eye = r.eyes[i];
        if (i) out << ',';
        out << "{\"width\":" << eye.width << ",\"height\":" << eye.height << ",\"samples\":" << eye.samples << '}';
    }
    out << "],\n  \"extensions\": [";
    for (size_t i = 0; i < r.extensions.size(); ++i) {
        if (i) out << ',';
        out << quote(r.extensions[i]);
    }
    return out.str() + "]\n}\n";
}
} // namespace hookshot::vr
