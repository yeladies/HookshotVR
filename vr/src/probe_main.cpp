// SPDX-License-Identifier: GPL-3.0-or-later
#include "probe.h"
#include <iostream>

int main() {
    const hookshot::vr::Dispatch api{xrEnumerateInstanceExtensionProperties, xrCreateInstance, xrDestroyInstance,
        xrGetInstanceProperties, xrGetSystem, xrGetSystemProperties, xrEnumerateViewConfigurations,
        xrEnumerateViewConfigurationViews};
    const auto report = hookshot::vr::probe(api);
    std::cout << hookshot::vr::to_json(report);
    return hookshot::vr::exit_code(report.status);
}
