# Isolated OpenXR/Vulkan smoke test

Windows x64 only. This builds a separate executable, not the game or its renderer.
No game data is read, copied or packaged. Runtime selection is left unchanged.

Three explicit stages are available:

| Arguments (in addition to `--loader`) | Work performed |
|---|---|
| none | Enumerate enable2, create XR instance, get HMD system, query Vulkan requirements |
| `--session` | Also create a Vulkan 1.3 instance/device through XR wrappers, choose the runtime GPU and create/destroy an idle session |
| `--quad-seconds 10` | Also run a brief XR frame loop displaying a dim blue 16:9 panel, then request exit and end the session |
| `--quad-seconds 20 --capture-pid PID` | MSVC build: show the explicitly selected Hookshot window in that quad through Windows Graphics Capture and a CPU/Vulkan upload |

The panel is head-relative, 1.6 by 0.9 metres at 2 metres distance, in both
eyes. Its default content is synthetic; the optional capture mode copies a running
Hookshot window. No controller actions or game camera transforms
are implemented. Session initialization does not validate the game's full Vulkan
feature chain. The runtime's reported GPU is used without changing desktop GPU options.

## Build

Use CMake 3.20+, a Windows x64 compiler, official OpenXR headers and Vulkan
headers. No OpenXR import library is needed: an explicitly selected, trusted loader
is loaded with a restricted Windows DLL search path. Do not pass the runtime DLL
(for example `virtualdesktop-openxr.dll`) as the loader.

Tested headers: OpenXR-SDK commit `24a1f2b92d212501a2b1b8d30ffd53abf55fcfa8`,
Vulkan-Headers 1.4.345. The headers are external build inputs and are not vendored.

From the repository root, with paths adjusted to your installed headers:

```powershell
git clone https://github.com/KhronosGroup/OpenXR-SDK.git work/OpenXR-SDK
git -C work/OpenXR-SDK checkout 24a1f2b92d212501a2b1b8d30ffd53abf55fcfa8
cmake -S experiments/pcvr -B work/pcvr-smoke -A x64 `
  -DOPENXR_INCLUDE_DIR="$PWD/work/OpenXR-SDK/include" `
  -DVULKAN_INCLUDE_DIR="C:/path/to/Vulkan-Headers/include"
cmake --build work/pcvr-smoke --config Release
ctest --test-dir work/pcvr-smoke -C Release --output-on-failure
```

The default Visual Studio generator must target an installed C++ toolchain. A
MinGW/Ninja generator with an explicitly chosen x64 compiler is also supported.
MSVC builds use C++20 and the Windows SDK C++/WinRT, D3D11 and Windows Graphics
Capture libraries. MinGW retains the C++17 synthetic/session stages and explicitly
rejects capture mode. Do not compile the game to build this test.

## Run

Start with the non-rendering stage; connect the HMD through the selected PC runtime
before trying `--session` or the panel:

```powershell
$loader = 'C:/absolute/path/to/trusted/openxr_loader.dll'
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader --session
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader --quad-seconds 10
```

For external game-window delivery, start a locally built Hookshot, obtain its PID
and explicitly choose it (the example variable must be assigned the actual PID):

```powershell
$gamePid = 12345 # replace with the PID of the intended running hookshot.exe
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --check-capture --capture-pid $gamePid
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader --quad-seconds 20 --capture-pid $gamePid
```

`--check-capture` inspects process identity/count and available memory only: it
does not load OpenXR, connect a headset or start Windows Graphics Capture. Capture
requires exactly one running `hookshot.exe`, matching the supplied PID, at least
768 MiB available physical RAM and 1024 MiB commit headroom. It refuses capture
before loading the XR loader if these checks fail. The memory figures are minimum
diagnostic reserves, not estimates of the game's total needs or an OOM guarantee.
Capture rechecks those conditions once per second between frames and stops on
failure. It never launches or kills game processes. Windows display configuration
is not changed by this executable. Holding Escape requests normal session exit.

Capture mode requires exactly one visible, non-owned window belonging to that
`hookshot.exe` process. It refuses a different executable, an ambiguous process,
a preparation console belonging to conhost, a minimized window or a resize.
It uses a per-window capture item, never a monitor or desktop capture. The system
capture notification border is not disabled. No captured pixels are written to
disk or sent to a network by this program; images travel to the selected XR runtime.

This is a diagnostic bridge, **not native renderer integration**: WGC produces
BGRA frames on D3D11, a CPU readback scales them with aspect-preserving letterboxing
to 640x360, and coherent Vulkan staging memory uploads them to acquired XR images.
The prior upload's fence completes before the buffer is rewritten. No-new-frame
reuses the last captured image; until the first capture arrives the blue panel is
shown. No game queue, DLL, executable, camera matrix or save file is patched.
The captured window includes its window decorations. This path is SDR, copies via
CPU, may add latency, does not forward controller input and is not a performance
baseline for future direct VkImage integration. Keep the game window restored and
its original controls focused as needed. A window close/resize stops the experiment.

Exit codes: 0 = completed requested stage; 1 = runtime/API failure; 2 = usage;
3 = HMD unavailable, no panel layers sent or no captured frames in capture mode;
4 = GPU fence timeout. A failed
requirements query is not a successful session test. A successful `xrEndFrame`
does not by itself establish that the user saw the panel.

The panel requires primary stereo, opaque composition and an enumerated format
with transfer-destination/color-attachment swapchain usage: sRGB for captured SDR
pixels, UNORM for the synthetic linear-color panel. Unsupported
conditions fail explicitly rather than guessing another presentation path. Each
frame clears an acquired/waited XR image, transitions it to the required color
attachment layout, waits for its Vulkan fence, releases it, then submits the quad
at predicted display time. `shouldRender=false` produces zero layers. Session
READY/STOPPING/EXITING/LOSS_PENDING and instance loss are handled. Normal shutdown
requests exit and ends on STOPPING; failures tear down the isolated session.

The simple one-frame-at-a-time fence wait intentionally favors auditability over
performance. A five-second GPU timeout terminates the process without attempting
to destroy resources still in use. Frame-loop duration is 1–60 seconds plus startup
and shutdown, subject to blocking runtime calls; this is not a general watchdog.
The test does not modify the Hookshot shared Vulkan queue or its synchronization.

## Recorded local result, 2026-11-08

- Compiled with MSVC 19.51 and Windows SDK 10.0.26100.0; also compiled with the
  existing llvm-mingw x64 compiler using warnings as errors.
- CTest help/relative-loader rejection passed; the original Python probe's eight
  unit tests passed.
- Before connection, the real runtime returned HMD unavailable (exit 3).
- After connection, VirtualDesktopXR 1.1.0 reported Vulkan minimum 1.1, tested
  maximum 2.0. Runtime GPU: NVIDIA GeForce RTX 2060 SUPER, Vulkan 1.4.351.
- Vulkan 1.3 device and idle XR session creation succeeded on family 0, queue 0.
- A ten-second panel run completed with 607 frame iterations and 606 submitted
  quad layers, reaching FOCUSED and ending through STOPPING; exit 0.
- The user explicitly confirmed seeing the panel in Quest 3. This confirms a
  synthetic visual test, **not game rendering, stereo gameplay or 6DoF integration**.

Vulkan/OpenXR validation layers, headset reconnection during rendering, device
loss and the renderer's full feature set have not been tested. No validation-layer
installation was found in the checked system registry location. Preserve the
original renderer until an opt-in game-frame copy path can be built and validated.

## Follow-up: external game window, 2026-11-08

The bridge captured an isolated copy of the user's pre-existing Hookshot v2.0.3
executable (its log identifies source `73ddf2ad`), not a newly built game executable
from this fork. The source audit remains pinned to the baseline stated in the docs.
The isolated run had its own `user` directory, read the existing local romfs, and
used the verified existing `RX_VK_PRECOMPILE=0` option for that process only. The
user's original shader-preparation run was left running. No game assets, binaries,
caches, saves or captured pixels are committed or included in the probe package.

A 20-second capture run with the same VirtualDesktopXR/RTX 2060 SUPER setup:

- Window source: 1282x752; output: 640x360, preserving aspect ratio.
- 685 frame iterations, 684 submitted quad layers, 572 newly captured frames.
- 351 changes in a sampled pixel hash (a changing-content indicator, not a frame
  rate or proof of correct colors). Repeated XR frames can reuse a captured image.
- Reached FOCUSED, requested exit, ended through STOPPING, exit 0.
- The user explicitly confirmed seeing the game image inside the Quest 3 panel.

Builds passed with MSVC capture enabled and llvm-mingw without capture. The existing
two CTest argument checks and eight Python tests still pass. Target selection also
rejected the preparation-only process before any capture. The first attempt from
the restricted execution environment could not inspect the process; the real
per-window run required the normal desktop process context. Normal desktop use
does not require administrator elevation.

This validates external window delivery only. Native Hookshot Vulkan-to-XR sharing,
stereo and game-camera 6DoF remain unimplemented. The next integration should
replace the CPU bridge only after preserving the game's device feature chain,
source image lifetime and shared queue synchronization under validation layers.

After the successful visual run, keeping the isolated game alongside the original
shader-preparation process exhausted system memory while starting a packaging
shell. The isolated game was closed; the original process was not stopped. This
is an observed resource limitation: use one game instance for subsequent tests.
The successful run is not evidence of stable long-session resource use.

## Recovery and guard validation

The user later reported a solid-blue physical monitor. No game/probe processes
remained. Refreshing graphics, selecting the PC-only Windows projection topology
and restarting Virtual Desktop did not restore the physical image. The user
reported recovery after checking the input and reconnecting the cable. The cause
is unconfirmed; no claim is made that the probe caused or fixed the signal issue.
The recovery actions were diagnostic host actions, not behavior of this program.

With Hookshot closed, the hardened build passed three CTest checks. The new
process-lifecycle check creates two harmless sleeping copies of its own test
executable named `hookshot.exe` inside the build directory. It verifies real
process enumeration, selected-PID matching, duplicate rejection, cleanup and
simulated RAM/commit exhaustion without loading XR or any game assets. It skips
instead of spawning fixtures if a real Hookshot process is already running.
The eight Python preflight tests still pass. A direct `--check-capture` invocation
with no game running returned `capture_blocked`, with no runtime initialization.
MSVC capture and llvm-mingw synthetic/session builds compile. The later headset
verification below exercised the periodic checks and Escape path.

## Guarded headset run and brightness correction

One isolated game instance passed preflight with roughly 4 GiB available RAM and
3.2 GiB commit headroom. A guarded ten-second run submitted 363 layers from 363
captures and ended by deadline. The user confirmed the physical monitor was normal
but reported excessive headset brightness.

The capture was incorrectly putting SDR desktop sRGB-encoded bytes into an XR
UNORM image. OpenXR treats those image values as linear. Capture now requires an
enumerated `B8G8R8A8_SRGB` or `R8G8B8A8_SRGB` swapchain and preserves encoded bytes,
swizzling BGRA/RGBA only. It explicitly fails without sRGB capture support. No
headset brightness setting or arbitrary darkening multiplier is applied. The
synthetic linear-color panel retains UNORM. See the
[Khronos swapchain color contract](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSwapchain.html).

The corrected run, with a 30-second ceiling, selected Vulkan format 50
(`VK_FORMAT_B8G8R8A8_SRGB`). The user pressed Escape: the log recorded
`exit_requested=escape`, 326 frame iterations, 325 layers and 245 captured frames,
then STOPPING and exit 0. The user confirmed normal brightness and a normal monitor
afterward. Periodic process/memory checks remained active. The task-created game
was closed normally and its exit verified.

All four CTest checks and eight Python tests passed after the correction. The new
CPU-only color regression covers sRGB selection when UNORM is listed first,
UNORM-only rejection, encoded channel preservation, alpha and BGRA/RGBA ordering.
Both compiler variants build. This is a short user-confirmed SDR visual result,
not HDR calibration, a long stability test or native renderer sharing validation.
Forced low-memory shutdown during live XR remains untested; policy failures were
exercised with fixtures.
