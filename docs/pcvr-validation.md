# PCVR experiment validation

## Recovery follow-up and capture resource guards

After a reported solid-blue physical monitor, the user restored its image after
checking the input/reconnecting the cable. At that point no game or probe process
remained. Graphics refresh, PC-only projection and a Virtual Desktop restart had
not recovered it. The cause is unconfirmed. These were host troubleshooting
actions; the experiment contains no monitor-topology or runtime-selection changes.

The capture path now requires one matching Hookshot process and minimum available
RAM/commit headroom before loading OpenXR, with once-per-second rechecks during
capture. A non-graphical `--check-capture` mode and an Escape exit request were added.
Three CTest checks and eight Python tests pass. The new CTest uses harmless child
process fixtures and simulated low memory; it does not launch Hookshot or XR.
A real no-game preflight was rejected before runtime initialization. Both compiler
variants build. No new headset run has validated the periodic guards or Escape.

## Follow-up: external window bridge

A separate 20-second Windows Graphics Capture test copied an isolated Hookshot
v2.0.3 window (installed executable's reported source `73ddf2ad`) through CPU
readback and Vulkan upload into the XR quad. This was not a rebuilt game from the
audited fork and did not patch its renderer. It used its own save directory and
the existing local romfs; game-derived files stayed in ignored local work.

Observed: 685 frame iterations, 684 XR quad layers, 572 captured window images,
351 sampled image-content changes, exit 0 after STOPPING. Source window 1282x752,
letterboxed output 640x360. The user explicitly confirmed seeing the game in Quest
3. MSVC capture build and MinGW synthetic-only build succeeded; existing CTest and
Python checks passed. A shader-preparation process was rejected because it had
no qualifying game window. Vulkan/OpenXR validation layers were not used.

This result supports external monoscopic window delivery only; it does not validate
game-frame VkImage sharing, game camera tracking, stereo, input, latency, colors
or performance. Reproduction and limitations: [compiled experiment](../experiments/pcvr/README.md).

Following the visual test, simultaneous isolated-game and original shader-preparation
processes exhausted system memory during packaging. Only the task-created game
copy was closed. Further tests must use one game instance; long-session stability
has not been established.

## Follow-up: compiled session and synthetic panel

After the initial audit below, tools were found outside PATH: Visual Studio 18
Community/MSVC 19.51, Windows SDK 10.0.26100.0, and Hookshot Builder's llvm-mingw
and Vulkan-Headers 1.4.345. OpenXR headers were fetched from the official SDK at
`24a1f2b92d212501a2b1b8d30ffd53abf55fcfa8`.

The installed OpenXR loader bundled with a Meta Horizon application successfully
loaded the selected VirtualDesktopXR runtime and advertised Vulkan enable2.
The compiled probe first returned HMD unavailable. After the user connected Quest
3, it created an XR instance, queried requirements, selected the RTX 2060 SUPER,
and created a Vulkan 1.3 device and idle XR session. A ten-second synthetic quad
run submitted 606 layers across 607 frame iterations and shut down through
STOPPING with exit 0. The user confirmed seeing the blue panel in Quest 3.

The executable also compiled with llvm-mingw with warnings treated as errors.
Two CTest argument checks and the eight Python probe tests passed. No game build
or game-frame delivery has been tested. No validation layers were used. Full
commands, limitations and device details: [compiled experiment](../experiments/pcvr/README.md).

## Initial source-only audit (historical results)

Date: 2026-11-08. Baseline: `45620e817d6000118d6077857ba2910d051de164`.
Environment: Windows x64, Python 3.12, fresh source clone. No C++ compiler found on
PATH; Vulkan/SPIR-V development headers and several optional test dependencies
were not discovered. No game build or headset run was performed.

| Check | Result |
|---|---|
| `python -I -B tests/test_openxr_preflight.py` | 8 unit tests passed; fake enumeration only |
| `python -I -B scripts/openxr_preflight.py` | Exit 1; default `System32/openxr_loader.dll` or a dependency could not be loaded |
| `git diff --check` | Passed |
| Full game-free runner, experimental tree | 43 passed, 36 failed, 28 skipped |
| Same runner, clean detached baseline worktree | 42 passed, 36 failed, 28 skipped |
| Comparison of failed check names | Identical sets; no newly failing check |

The full-run totals count test scripts/checks, not individual unit test methods.
The experimental run included the new probe test script; its final eight-method
version was also executed directly after adding three further error-boundary cases.

Reproduce the general comparison with a clean baseline worktree and a writable
temporary folder. For each tree, from its root:

```powershell
New-Item -ItemType Directory -Force work/pcvr-tmp | Out-Null
$env:TEMP = (Resolve-Path work/pcvr-tmp).Path
$env:TMP = $env:TEMP
python -X utf8 -I -B tests/run_all.py --jobs 4 --timeout 60
```

The initial default temporary directory was not writable for several checks.
Redirecting TEMP/TMP improved execution. UTF-8 output was needed to keep the
baseline runner's failure reporting from crashing on a replacement character.
Remaining failures include missing compiler executables, Windows link-fixture
permissions, and a `Path.read_text(newline=...)` argument unsupported by Python
3.12. The baseline comparison establishes these are reproducible without the XR
addition; it does not make the whole suite green or prove all failures harmless.
No unrelated test or runtime behavior was changed to hide those failures.

The direct probe result establishes only a failure to load the default DLL in this
environment. No alternate DLL was supplied; the installed runtime inventory was
not established. There is **no evidence here** of working OpenXR initialization,
Vulkan sharing, XR swapchains, tracking, controllers, stereo or headset display.

The implementation deliberately stops at extension enumeration. An in-game
session prototype needs a real SDK/compiler and runtime validation before changing
the renderer's device creation or shared queue scheduling. The next visible
prototype is specified in [pcvr-experiment.md](pcvr-experiment.md).
