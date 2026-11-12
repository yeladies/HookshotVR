# VR bootstrap validation

Date: 2026-11-11. Source baseline: `940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`.

## New VR code

- Windows x64 build: passed with llvm-mingw clang 23.1.5 and CMake/Ninja.
- Official Khronos OpenXR-SDK 1.1.65 pinned by commit; static loader built locally.
- Native contract test: passed, including ten groups of capability/error/cleanup/output checks.
- Standalone launch initially exposed a dependency on compiler runtime DLLs; the MinGW targets
  now link statically and run from a plain terminal without a compiler PATH.
- Live runtime, visor disconnected: instance creation and extension discovery worked;
  `xrGetSystem` returned `XR_ERROR_FORM_FACTOR_UNAVAILABLE`, reported as exit 3.
- Live runtime, visor connected through Virtual Desktop: exit 0, runtime `VirtualDesktopXR`,
  system `Meta Quest 3`, `XR_KHR_vulkan_enable2` present, orientation and position tracking
  advertised, two primary-stereo views at recommended 2112 x 2304, one sample each.

The recommended view size is not a measured performance target. No actual pose samples,
session, controller actions, Vulkan device, rendered frames or game integration were tested.
Linux/MSVC builds are configured for CI but have not yet been observed passing.

## Inherited suite

`python -I -B tests/run_all.py --jobs 2` completed: **151 passed, 58 failed, 211 skipped**.
This ran against unchanged upstream game/runtime/renderer code in the same checkout.
The environment does not provide the compiler names expected by many inherited tests;
examples fail trying to invoke `g++`, while others are skipped by tool/header discovery.
Other observed failures include .NET restore/run fixtures and Windows symlink privilege errors.
Not every failure has been classified. These results do not establish a clean upstream baseline.

The new VR CMake tests are independent and pass; this does not clear the inherited failures.
Keep the work on a development branch rather than describing it as a validated game build.
Before changing the renderer, prepare the upstream test toolchain and resolve or reproduce the
remaining failures against an unmodified baseline. No unrelated tests were weakened or edited.

## Installation preservation

The stable installation was not launched, updated or written by the bootstrap tools.
Its launcher, game executable, renderer, version/configuration and save-file hashes are checked
against the previously recorded local baseline. Raw logs and reports remain in ignored `work/`.

## Spatial session follow-up

The new `hookshot_vr_scene` built successfully on Windows with the same compiler. Both native
test executables passed, including projection/roll/disparity/validity checks. Vulkan headers
are pinned independently; the loader is loaded dynamically from the OS.

The first 20-second live run created the Vulkan-bound OpenXR session on the runtime-selected
RTX 2060 SUPER. It received 1,422 valid head poses and stereo-view samples. Observed head position
ranges were approximately 0.36 m, 0.69 m and 0.60 m in the local X/Y/Z axes.

That run remained in SYNCHRONIZED without VISIBLE or FOCUSED. `shouldRender` remained false:
**zero stereo frames were submitted, and zero controller poses were recorded**. The application
did not force rendering. This establishes live head pose access, not visible stereo or controller
tracking. A follow-up run with the headset worn and Virtual Desktop switched to VR is required.
The revised startup waits up to 45 seconds for visibility before counting the visible test time.

A second run used that 45-second wait. It collected 3,227 valid head/view samples and emitted
the last head position and quaternion, but again submitted zero stereo frames and obtained zero
controller poses. The application exited with code 3 and explicitly reported the visibility timeout.
The position ranges are observations of runtime pose data, not a controlled movement experiment;
startup/recentering changes have not been separated from physical headset movement.

Visible output and controller tracking remain **unverified**. The native scene code is available
for the next interactive test; no successful headset visual test is claimed.

## Successful interactive retry — 2026-11-12

A subsequent 10-second visible test completed with exit 0. The runtime selected the RTX 2060 SUPER,
reached VISIBLE and FOCUSED, and accepted 705 stereo projection frames from 706 frame iterations.
The run recorded 706 valid head/view samples and 618 valid grip-pose samples for each controller.
The user confirmed seeing the scene in the Quest headset and that the desktop monitor continued
working. The application closed normally; no scene process remained active afterward.

This supersedes the earlier unverified status for basic headset output and controller grip-pose
access. It does not validate game integration, physical combat, positional accuracy, latency,
full renderer feature compatibility or all disconnect/device-loss paths. The user did not yet
separately assess whether the world markers remain stable under every head movement.

Before this retry, the user reported that the desktop monitor had previously become blue with no
signal and required reconnecting HDMI. The cause is undetermined; this was not described as a
Windows blue-screen crash. The issue did not recur in this retry according to the user. Do not
infer that all display/driver failure cases have been resolved. Raw poses and logs remain local.

## Isolated native game build and menu smoke — 2026-11-12

The audited source baseline built locally using the supported private game inputs and Builder
optimization metadata. The profiled native compile completed in 3,394 seconds with zero failed
compilation units, then linked a matched game executable and Vulkan renderer. This establishes
buildability in this Windows toolchain, not a clean inherited test suite or game correctness.

The first 60-second headless smoke was spent preparing shaders. The stable native program cache
has a different compatibility stamp and was not reused. A second 120-second smoke used the
renderer-supported on-demand shader mode (`RX_VK_PRECOMPILE=0`), private low-quality options,
1280x720 game output, muted audio and private user/cache paths. It reached the Spanish main menu;
the final inspected capture shows the title and New Game/Options entries. More than 6,300 presents
were logged. The final interval reported about 59.8 fps and zero failed shader programs; these are
menu observations, not gameplay or VR performance. Cold pipeline compilation caused long stalls.
Peak sampled process working set was 3,349 MiB; this excludes total system/GPU memory.

The wrapper stopped its own process at the requested time limit; this was not a natural game exit.
No test game process remained. The 19 previously inventoried stable executable, library, configuration
and save files still matched their SHA-256 baseline. Application-controlled state stayed under
ignored experimental work directories; graphics-driver-managed caches may live elsewhere.

Gameplay, save compatibility and stereo game rendering remain unverified. The standalone OpenXR
scene and this flat game build are still separate. Next validate a private gameplay session, then
integrate runtime-selected Vulkan device negotiation and per-eye camera/projection without replaying
simulation or write-producing GPU work twice. Raw logs, captures, binaries and caches stay local.

## Private save-load and input follow-up — 2026-11-12

The runner now offers an explicit stable-save snapshot option. Each run makes a new private user
folder, verifies copied file hashes, and passes that folder to the game; original saves are never
passed as writable runtime paths. The mounted save path in the local log confirmed this separation.

A bounded 240-second headless run loaded a copied save into Dodongo's Cavern. Captures showed the
3D scene, Link and HUD. A second 140-second run loaded a fresh snapshot and accepted live scripted
left-stick movement and right-stick camera input. Inspected captures showed displacement and a
changed view. This is a narrow load/input/render smoke, not a complete gameplay or save-compatibility
assessment. The runner retains `gameplay_verified=false` rather than automatically certifying play.
Both runs ended at their intended time limit. Peak sampled working sets were 6,924 and 6,771 MiB.
Selected five-second gameplay intervals in the second run reported roughly 37–42 fps, with cold
shader stalls also observed. This is flat headless output, not headset performance or a VR budget.

The 19 inventoried stable files retained their original SHA-256 hashes. Logs, snapshots and captures
remain local under ignored work folders. A separate synthetic `vr_camera_math` test now covers
translation/recenter/scale, yaw/pitch/roll, stereo eye offset and invalid samples; all three standalone
VR CTest targets pass locally. This math is not yet connected to the game's camera or renderer; see
[camera integration](vr-camera-integration.md) for the exact boundary and next gates.

## Renderer negotiation follow-up — 2026-11-12

The production Vulkan renderer now has an explicit SDK build option and runtime opt-in for OpenXR
device negotiation. Its existing Vulkan instance/device configurations are passed through enable2;
the runtime-selected physical device overrides the desktop GPU preference. This does not change
the version-17 host/renderer ABI or enable a session, stereo targets, game poses or frame submission.

The SDK-enabled renderer compiled with zero failed units. An initial monitored smoke hit a log-file
sharing error in the PowerShell wrapper after negotiation had succeeded; its finally block stopped
the owned process. The wrapper now reads the live log with read/write sharing. The repeated bounded
45-second headless smoke reported `xr_device_negotiated=true`, selected the RTX 2060 SUPER and
continued Vulkan replay to the main menu (capture inspected). Peak sampled working set: 3,097 MiB.
A subsequent 35-second smoke of the same adapter with XR disabled used Vulkan without XR log markers
and peaked at 3,097 MiB. Both ended at their deliberate time limit; neither opened an XR session.

All 13 native VR CTest cases pass locally, including the production adapter against synthetic loader
errors and the no-SDK stub. The Windows renderer build also links successfully with the official
shared loader loaded by an explicit path; driver/runtime version changes remain separate variables.
The full inherited suite with the MinGW tools on PATH completed **356 passed, 5 failed, 59 skipped**
in 313 seconds. All five failures reproduced with the same symptoms on a separate source-only archive
of unchanged upstream `940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`:

- `test_fpx_sticky`: the fixture compiler cannot find the C++ `algorithm` header.
- Two Builder fixtures: .NET 8 reference/host packs unavailable to their restore/run configuration.
- `test_nup_paths`: Windows does not grant the fixture's symlink creation privilege.
- `test_vram_budget_row`: an existing unused-constant warning is treated as a compile error.

No inherited tests were weakened or edited. The 59 skips still limit coverage; this is not a fully
green upstream suite. The 19 inventoried stable installation files retained their SHA-256 hashes,
and no game test process remained after the bounded runs. Raw logs and all build/game-derived output
remain local. See [renderer negotiation](vr-renderer-openxr.md) for commands and the next boundary.
