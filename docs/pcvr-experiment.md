# Experimental PCVR starting point (Quest 3)

Audited baseline: `45620e817d6000118d6077857ba2910d051de164` on
`yeladies/HookshotVR`, 2026-11-08. Work branch:
`experiment/pcvr-openxr-preflight`. This is a loader diagnostic and an integration
plan, **not a VR game build**. A later isolated synthetic panel test was displayed
in Quest 3 and confirmed by the user. A subsequent Windows Graphics Capture bridge
also delivered an isolated Hookshot window to the panel, confirmed by the user.
Native render-to-XR integration, stereo gameplay and XR controllers remain untested.
See [the compiled experiment](../experiments/pcvr/README.md).
No game-derived files are required by either probe.

## Reproduce the first experiment

```powershell
git clone https://github.com/yeladies/HookshotVR.git
cd HookshotVR
git switch experiment/pcvr-openxr-preflight
python -I -B tests/test_openxr_preflight.py
python -I -B scripts/openxr_preflight.py
```

Requires Windows x64 Python 3 and a trusted x64 OpenXR loader. The default probe
checks `%SystemRoot%/System32/openxr_loader.dll`; a loader is not necessarily
installed there. If it lives elsewhere, pass its **absolute** location:

```powershell
python -I -B scripts/openxr_preflight.py --loader 'C:\absolute\trusted\openxr_loader.dll'
```

The script does not install a loader, change runtime selection, edit registry or
configuration files, launch the game, create an instance/session or allocate Vulkan
resources. Loading the DLL and enumerating extensions may load the active runtime.
JSON goes to stdout. Exit 0 means only that `XR_KHR_vulkan_enable2` is advertised;
1 means the probe failed or that extension is absent; 2 means invalid arguments.
Keep the loader/runtime versions with any report. A missing default DLL does not
prove there is no runtime installed. An advertised extension does not prove Quest
connectivity, Vulkan 1.3 compatibility, graphics device compatibility or frame delivery.

For a later headset run, establish the PC streaming connection and select the intended
PC OpenXR runtime using its own controls. This project targets rendering on the PC,
not a Quest Android APK. Runtime selection and transport remain manual test setup.

## Verified renderer integration map

Paths and symbols below exist in the audited commit. Comments in some headers
still say MKW or WebGPU; the actual native Vulkan path is distinct.

| Existing code | Current responsibility | Proposed future XR integration |
|---|---|---|
| `host/rx_backend.c`: `rx_start`, `rx_nvn_present`, `load_vulkan` | Loads `hookshot_vk.dll`, selects a renderer, forwards NVN presents | Keep desktop default; make XR explicitly opt-in and log failure separately from WebGPU fallback |
| `gpu/vk/vk_api.h`: `MkwVkHost`, `MkwVkRenderer`, `MKWVK_API_VERSION` (4) | Host/DLL contract | Avoid ABI changes for internal XR state; version any later cross-boundary changes |
| `gpu/vk/vk_main.cpp`: `mkwvk_init` | Calls `create_instance`, `pick_device`, `create_device` before window startup | Initialize XR and query graphics requirements **before** this Vulkan creation sequence |
| Same file: `create_instance` | Requires Vulkan 1.3; creates a desktop Vulkan instance | Use `xrCreateVulkanInstanceKHR` on the XR path, preserving application requirements and checking both XR and Vulkan results |
| Same file: `pick_device` | Selects a Vulkan 1.3 adapter by preference/score | Use `xrGetVulkanGraphicsDevice2KHR`; the runtime's physical device must win over desktop GPU preference |
| Same file: `create_device` | Builds feature chain, extensions and queue | Use `xrCreateVulkanDeviceKHR` after checking existing replay requirements; preserve enabled features and retain the queue index |
| Same file: `start`, `create_swapchain`, `resize` | Window surface/swapchain, or offscreen mode | Separate XR swapchains from desktop WSI and mirror resizing; offscreen mode alone does not implement XR |
| `gpu/vk/vk_internal.h`: `Device` | Vulkan handles, queue family, swapchain images and `present_mu` | Add separately owned XR instance/session/spaces/swapchains; never destroy runtime-owned images as app allocations |
| `gpu/vk/vk_replay.cpp`: `do_present`, `present`, `present_image` | Finds final NVN texture, applies presentation path and submits to WSI | Candidate hook for a flat virtual screen after source readiness, before desktop acquire/present |
| Same file: `blit_crop`, `blit_crop_to_rect` | Copies/crops the final image | Reuse only after validating XR image usage, format, bounds, layout and queue synchronization |
| `gpu/vk/vk_framegen.cpp` and `vk_replay.cpp` framegen calls | Desktop interpolation, scaling and presentation scheduling | Start the future XR path without desktop frame generation; XR timing must drive frame delivery |
| `gpu/render/rx_core.cpp`, `rx.h` | WebGPU fallback implementation | Not the primary hook for the native Vulkan experiment |
| `nx2/build.py`: Vulkan backend build path | Builds `hookshot_vk.dll`/`.so` with external dependencies | Add an optional SDK dependency only when an XR session implementation can be compiled/tested; do not force it on desktop builds |

## Next implementation gate: flat screen first

The lowest-risk visible experiment is the existing monoscopic frame on an
`XrCompositionLayerQuad`. This would be a virtual screen, not stereoscopic gameplay.
The final frame already includes HUD, so it needs no speculative camera patch.
The isolated experiment now implements session/device creation and a synthetic quad
frame loop. An external per-window/CPU bridge can now display a running game's
window. Directly copying the renderer's final Vulkan texture remains unimplemented.

1. Enable `XR_KHR_vulkan_enable2`, create an XR instance, obtain an HMD system and
   call `xrGetVulkanGraphicsRequirements2KHR`. Compare its OpenXR-encoded version
   values with the renderer's Vulkan 1.3 requirement; do not compare raw Vulkan
   version integers against `XrVersion`. Record the reported tested maximum.
2. Create Vulkan instance/device through the runtime wrappers, obtain its physical
   device, and validate the replay's features on that device. Resolve extension
   functions through `xrGetInstanceProcAddr`. Use `XrGraphicsBindingVulkan2KHR` with
   the same instance, device, queue family and queue index when creating the session.
3. Enumerate supported formats; create a transfer-destination-capable XR swapchain
   only if the runtime supports the intended copy/blit path. Otherwise implement a
   render pass with a supported usage. Do not assume desktop BGRA, size or image count.
4. Poll XR events; begin on READY, end on STOPPING, handle EXITING/LOSS_PENDING.
   Use `xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`, including zero layers when
   `shouldRender` is false. Acquire/wait/release XR images in their own lifecycle.
   Set explicit quad pose/size and a reference space; update timing independently
   of game simulation. Account for reference-space changes and session loss.
5. Protect shared Vulkan queue access with the existing queue synchronization policy
   (`present_mu`/`queue_flush_locked`). Verify source completion, image layouts and
   copy completion before releasing the runtime image; do not transfer desktop WSI
   binary semaphore ownership to XR. Tear down XR session/swapchains before the
   shared Vulkan device. Keep desktop mirror and XR frame lifetimes separate.

True stereo/6DoF is a later research task: `vk_draw.cpp` replays translated draws
and constant buffers; this audit did not identify a verified game-specific camera
matrix or world-unit scale. Rendering the same final texture twice cannot produce
stereo. Determine camera transforms, per-eye asymmetric projections, HUD treatment,
culling and camera collision using a local supported game build before attempting it.

## Validation boundaries

The probe uses only the standard library. Its one ABI structure and enum values
come from the [Khronos OpenXR header](https://github.com/KhronosGroup/OpenXR-SDK/blob/main/include/openxr/openxr.h).
The future initialization sequence follows
[XR_KHR_vulkan_enable2](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/extensions/khr/khr_vulkan_enable2.adoc).
The tests mock enumeration; they do not substitute for a real loader/runtime run.

Local results and remaining checks are recorded in [pcvr-validation.md](pcvr-validation.md).
Before any claim of headset support, require a recorded session startup/shutdown,
runtime-selected GPU and feature report, validation-layer run, repeated reconnect,
focus loss, resize and session-loss tests, plus an actual visible Quest 3 frame.
Do not distribute generated game code, binaries, caches, dumps or saves.
