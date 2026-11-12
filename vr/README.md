# HookshotVR development

Experimental PCVR adaptation for Quest 3 on Windows, initially through Virtual Desktop / VDXR.
**The game is not playable in VR yet.** This first milestone adds a game-free OpenXR capability probe, not a VR renderer.
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
