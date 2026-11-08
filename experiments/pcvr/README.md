# Isolated OpenXR/Vulkan smoke test

Windows x64 only. This builds a separate executable, not the game or its renderer.
No game data is read, copied or packaged. Runtime selection is left unchanged.

Three explicit stages are available:

| Arguments (in addition to `--loader`) | Work performed |
|---|---|
| none | Enumerate enable2, create XR instance, get HMD system, query Vulkan requirements |
| `--session` | Also create a Vulkan 1.3 instance/device through XR wrappers, choose the runtime GPU and create/destroy an idle session |
| `--quad-seconds 10` | Also run a brief XR frame loop displaying a dim blue 16:9 panel, then request exit and end the session |

The synthetic panel is head-relative, 1.6 by 0.9 metres at 2 metres distance, in both
eyes. It contains no game imagery. No controller actions or game camera transforms
are implemented. Session initialization does not validate the game's full Vulkan
feature chain. The runtime's reported GPU is used without changing desktop GPU options.

## Build

Use CMake 3.20+, a Windows x64 C++17 compiler, official OpenXR headers and Vulkan
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
Do not compile the game to build this test.

## Run

Start with the non-rendering stage; connect the HMD through the selected PC runtime
before trying `--session` or the panel:

```powershell
$loader = 'C:/absolute/path/to/trusted/openxr_loader.dll'
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader --session
& work/pcvr-smoke/Release/hookshot_xr_smoke.exe --loader $loader --quad-seconds 10
```

Exit codes: 0 = completed requested stage; 1 = runtime/API failure; 2 = usage;
3 = HMD unavailable or no panel layers sent; 4 = GPU fence timeout. A failed
requirements query is not a successful session test. A successful `xrEndFrame`
does not by itself establish that the user saw the panel.

The panel requires primary stereo, opaque composition and an enumerated UNORM
format with transfer-destination/color-attachment swapchain usage. Unsupported
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
