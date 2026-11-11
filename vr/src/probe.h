// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openxr/openxr.h>
#include <cstdint>
#include <string>
#include <vector>

namespace hookshot::vr {
// Read-only capability discovery. No session, Vulkan device or game access.
struct Dispatch {
    PFN_xrEnumerateInstanceExtensionProperties enumerate_extensions;
    PFN_xrCreateInstance create_instance;
    PFN_xrDestroyInstance destroy_instance;
    PFN_xrGetInstanceProperties instance_properties;
    PFN_xrGetSystem get_system;
    PFN_xrGetSystemProperties system_properties;
    PFN_xrEnumerateViewConfigurations enumerate_view_types;
    PFN_xrEnumerateViewConfigurationViews enumerate_views;
};
enum class Status { ready, runtime_error, hmd_unavailable, unsupported };
struct EyeSize { uint32_t width, height, samples; };
struct Report {
    Status status = Status::runtime_error;
    std::string stage;
    XrResult result = XR_SUCCESS;
    std::string runtime;
    std::string system;
    XrVersion runtime_version = 0;
    bool vulkan_enable2 = false;
    bool orientation_tracking = false;
    bool position_tracking = false;
    std::vector<std::string> extensions;
    std::vector<EyeSize> eyes;
};
Report probe(const Dispatch& api);
int exit_code(Status status);
std::string to_json(const Report& report);
} // namespace hookshot::vr
