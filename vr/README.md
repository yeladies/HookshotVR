# HookshotVR development

Experimental PCVR adaptation for Quest 3 on Windows, initially through Virtual Desktop / VDXR.
**The game is not playable in native VR yet.** The tools now include frozen and moving depth-reconstructed previews, verified offscreen; their headset checks remain pending.

**Project pages:** [English](../docs/vr-guide/en/index.md) · [Castellano](../docs/vr-guide/es/index.md).

The sections below retain the setup and technical history, starting with the game-free OpenXR probe.
The inherited Hookshot source and licenses remain intact. See [the roadmap](../docs/vr-roadmap.md).
A standalone [Vulkan stereo marker scene](../docs/vr-spatial-test.md) now exercises sessions,
per-eye projection and live head/controller grip poses independently of the game.

## Build and test the probe on Windows

Requirements: Git, CMake 3.24+, Ninja, C/C++17 compiler. `build.ps1` can use the existing
Hookshot Builder's llvm-mingw in read-only mode, or an explicit `-ToolchainRoot`.
It downloads the official Khronos OpenXR SDK 1.1.65 at commit
`24a1f2b92d212501a2b1b8d30ffd53abf55fcfa8` into ignored `work/OpenXR-SDK`.
The SDK's Apache-2.0 notices and third-party notices stay with that checkout.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build.ps1
.\work\vr-build\hookshot_vr_probe.exe
.\work\vr-build\hookshot_vr_scene.exe --seconds 20
```

The build script runs the native contract tests without a headset or game.
For a capability check, connect Quest through Virtual Desktop first, then run the probe.
The executable writes JSON to stdout; Khronos loader diagnostics may also appear on stderr.
Exit codes: 0 = required capabilities advertised; 2 = runtime/API error;
3 = headset unavailable; 4 = required Vulkan, stereo or tracking capability missing.

The probe enumerates extensions, enables `XR_KHR_vulkan_enable2`, creates an instance,
queries the HMD system and its stereo view sizes, then destroys the instance.
It does not create a session, query live poses, open the game, change the active runtime,
or write to the game installation. A result of `capabilities_ready` does not prove graphics
device compatibility, live tracking, controller input, frame submission or performance.
The separate `hookshot_vr_scene` performs those first session/render/pose experiments; its output
and limitations are documented separately. Neither tool starts or modifies Hookshot.

For Linux or another compiler:

```sh
cmake -S vr -B work/vr-build -DCMAKE_BUILD_TYPE=Release
cmake --build work/vr-build --parallel 2
ctest --test-dir work/vr-build --output-on-failure
```

CMake fetches the same SDK revision unless `OPENXR_SDK_SOURCE_DIR` supplies a local checkout.
Reports, SDK files, executables and test output belong under ignored `work/`.
Only original mod source, tests and documentation are published. Never add game files,
generated game code, shader caches, saves or personal configuration to Git.

## Isolated game build

See [the flat baseline procedure](../docs/vr-game-build.md) for `build-game.ps1` and
`run-game.ps1`. These use a private work/runtime root and keep the stable installation as
read-only input. Future physical interaction research uses the saved
[reference projects](../docs/vr-reference-projects.md).

See [camera integration](../docs/vr-camera-integration.md) for the tested 6DoF camera math and
the remaining host/renderer connection. The game does not consume XR poses yet.

The optional [renderer negotiation adapter](../docs/vr-renderer-openxr.md) now creates the actual
renderer Vulkan device through OpenXR enable2. It remains opt-in and does not submit game frames
to the headset. `build-renderer.ps1` rebuilds that component without translating or compiling the game.

The opt-in [renderer session probe](../docs/vr-renderer-session.md) adds a bounded zero-layer
OpenXR frame loop and synchronized head/eye pose mailbox. It still does not display game stereo.

[Offline camera diagnostics](../docs/vr-offline-camera.md) provide ABI-18 read-only observations and
tested per-eye projection math. They can run without a headset and do not open an XR session.

## Game-image preview and update workflow

A bounded [flat-screen headset preview](../docs/vr-headset-preview.md) is implemented
as the next transport test. Real game readback and VDXR flat-quad visibility have been confirmed. It does not implement native stereo or head-driven gameplay.

[Upstream update tooling](../docs/vr-upstream-updates.md) prepares isolated source
candidates for explicit review. Version profiles and private build receipts prevent
launching unknown game builds or stale/mixed experimental binaries.

[Submitted scene-camera diagnostics](../docs/vr-scene-camera.md) now compare actual
recorded draw uniforms with the host camera without overriding any matrices.

[Resource lineage](../docs/vr-image-lineage.md) and [private uniform audits](../docs/vr-camera-uniform-audit.md)
prepare per-eye isolation. These remain diagnostic and do not render native game stereo.

The optional [SPIR-V uniform audit](../docs/vr-shader-uniform-audit.md) checks representative
shader reads of source-checked camera bindings; modules remain private and unchanged.

The [standalone owned-eye Vulkan experiment](../docs/vr-eye-vulkan-resources.md)
checks independent allocations and transfer/fence lifetime without a game or XR
session. It is not connected to the game's stereo replay path.

For compute-only diagnostics without opening a headset session, use
`vr/run-game.ps1 -ComputeShaderUniformAudit` with the existing isolated-run options.
[Visibility and replay gates](../docs/vr-culling-and-replay.md) cover CPU selection,
GPU culling/indirect data and the required single-simulation stereo model.

The [conservative stereo candidate planner](../docs/vr-stereo-candidate-plan.md)
combines compute-camera observations with a graph captured in the same process.
Its output is an incomplete resource plan, never permission to replay GPU work.

The [native game-image clone probe](../docs/vr-game-image-clone-probe.md) optionally
checks strict local-memory allocation, copies and timeline retirement from a real
scene draw using `-EyeImageProbe`. It does not display the copies or enable stereo.
