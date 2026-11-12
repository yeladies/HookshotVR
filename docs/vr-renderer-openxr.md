# Renderer OpenXR device negotiation

The optional adapter in `gpu/vk/vk_xr.cpp` now participates in the real renderer's initialization.
Negotiation alone creates no XR session. The separate opt-in [session probe](vr-renderer-session.md)
adds a bounded zero-layer session and pose mailbox. The game continues
using flat presentation even when negotiation succeeds. The host/renderer ABI remains version 17;
no fields or callbacks were added.

## Build and run locally

Use the same pinned official SDK checkout as the standalone tools. No SDK download, runtime install
or game build is performed by this renderer-only command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-renderer.ps1 -OpenXRSdkRoot work/OpenXR-SDK
```

Without `-OpenXRSdkRoot`, the adapter compiles as a stub and rejects explicit XR requests. With the
SDK, only the adapter object receives its headers/compile definition. SDK header content participates
in that object's cache key. Normal flat startup does not load OpenXR or require a connected headset.

The renderer loads the official loader by the absolute `HOOKSHOT_OPENXR_LOADER` path. On Windows it
limits dependency searches to that DLL's directory and System32; it does not search the working
folder or PATH. A local shared loader can be built from the pinned SDK using the existing CMake
project, with the same compiler/toolchain used for the native renderer:

```powershell
# Local Windows/MinGW toolchain used for the validated build:
$tools = Join-Path $env:LOCALAPPDATA 'HookshotBuilder/versions/v2.1.0/HookshotBuilder/tools'
$env:PATH = (Join-Path $tools 'llvm-mingw/bin') + ';' + $env:PATH
$sdk = (Resolve-Path work/OpenXR-SDK).Path
cmake -S vr -B work/vr-loader -G Ninja -DHOOKSHOT_VR_SHARED_LOADER=ON "-DOPENXR_SDK_SOURCE_DIR=$sdk" "-DCMAKE_C_COMPILER=$tools/llvm-mingw/bin/clang.exe" "-DCMAKE_CXX_COMPILER=$tools/llvm-mingw/bin/clang++.exe" "-DVULKAN_HEADERS_SOURCE_DIR=$tools/cpm/vulkanheaders/v1.4.345" -DCMAKE_BUILD_TYPE=Release
cmake --build work/vr-loader --target openxr_loader --parallel 2
$loader = (Resolve-Path work/vr-loader/openxr-sdk/src/loader/libopenxr_loader.dll).Path
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 45 -XrNegotiation -OpenXRLoader $loader
# Exercise that renderer with XR off:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 35 -ExperimentalXrRenderer
```

Use the tool paths/SDK revisions present on the machine; the explicit versions above describe the
validated local setup. A different generator/platform may produce a different loader filename or
configuration subdirectory. The runner's default loader path matches the MinGW/Ninja output above.
The CMake option removes the SDK's ELF version-script flag for MinGW shared-loader builds, using
its existing Windows export declarations instead, and links the compiler runtimes statically.
Other platforms' flags remain unchanged.
The runner copies the experimental renderer into the private runtime beside the previously built
experimental game. It does not replace any installed binaries. It sets `RX_VK_XR=1` only with
`-XrNegotiation`, and clears the loader path for ordinary flat runs. A successful negotiation marker
is required for a requested smoke to pass; the runner detects the host's ordinary WebGPU fallback
and stops its own process. Directly running the game outside this wrapper still retains that upstream
fallback behavior. Adapter failure alone must never be reported as a working VR session.

## Negotiation order

1. Load the explicitly selected official loader and require `XR_KHR_vulkan_enable2`.
2. Create an XR instance, obtain the HMD system, and call `xrGetVulkanGraphicsRequirements2KHR`.
   Require its advertised version range to include the renderer's Vulkan 1.3 request.
3. Pass the renderer's existing instance configuration through `xrCreateVulkanInstanceKHR`.
4. Ask `xrGetVulkanGraphicsDevice2KHR` for the required GPU. Match it against the renderer's available
   Vulkan 1.3 devices; this choice overrides desktop/menu GPU preferences.
5. Preserve the renderer's feature chain, extensions and queue configurations while passing its
   device creation through `xrCreateVulkanDeviceKHR`. Check both XrResult and VkResult.

No graphics work is repeated. Runtime-required device/instance additions are handled by enable2;
the renderer's normal feature checks still apply. Failed initialization resets the adapter. Full
session/device-loss teardown and reinitialization are subsequent work, not validated by this stage.

## Evidence and next boundary

The local Windows renderer compiled with zero failed units and negotiated the RTX 2060 SUPER via
the active VDXR runtime in a bounded 45-second headless game run. Vulkan replay continued and an
inspected capture reached the main menu. No XR session was created and no game frames were sent to
the headset. This does not establish stereo compatibility, latency, performance or gameplay safety.

Thirteen native CTest cases pass: prior probe/camera/projection cases, production adapter success,
missing extension/headset/function, incompatible version range, separate XR/Vulkan instance/device
errors, and a no-SDK build. The adapter cases use a synthetic shared loader without a runtime/GPU.
Live evidence and inherited-suite limitations are recorded in [validation](vr-validation.md).

Next connect a renderer-owned session to the negotiated device/queue, explicit frame ownership and
predicted-time pose samples. Per-eye camera/projection hooks and graphics replay isolation remain
as described in [camera integration](vr-camera-integration.md).
The follow-up [session/pose probe](vr-renderer-session.md) now implements the diagnostic session
portion of this next boundary; game camera consumption and stereo images remain pending.
