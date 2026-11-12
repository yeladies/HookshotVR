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

## Renderer-owned session follow-up — 2026-11-12

The optional renderer session probe is now implemented with a single owner thread and a synchronized
internal pose mailbox. This is still a zero-layer diagnostic: there are no game-eye swapchains,
head-driven game camera writes or game images in the headset. The host/renderer ABI remains 17.
Frame begin/end calls share the graphics queue's existing mutex; frame waiting does not hold it.
The first attempted build was interrupted before launching the live probe to add this queue
synchronization. No unprotected session test was run.

The first completed live run reached VISIBLE and FOCUSED and closed its session automatically after
686 balanced zero-layer frames, with 686 valid head and stereo-view pose samples. After adding host
observation timestamps/sample-expiry handling, the final renderer compiled in 63 seconds with zero
failed units. A repeated 25-second headless game smoke completed its bounded session with **697
frames, 697 valid head samples and 697 valid stereo-view samples**. The wrapper reported no XR API
errors, valid poses and session closure. Peak sampled process working set was 3,087 MiB. These counts
are runtime validity observations, not physical tracking accuracy or evidence of stereo game images.
No controller action integration was added in this step.

A subsequent 25-second smoke of that renderer with XR disabled used Vulkan with no XR log markers
and peaked at 3,061 MiB. Both game runs ended at their deliberate time limit. The session owner had
already destroyed its spaces and session in the XR run; the process-level watchdog is still a forced
stop, not a graceful full-engine teardown test. No game process remained after testing. The stable
installation's 19 inventoried files retained their SHA-256 hashes.

All **31 native CTest cases pass**, including production session lifecycle/partial cleanup, invalid
or stale samples, shouldRender false, focus/session/instance loss, origin revision, bounded restart,
frame balance and graphics-queue lock requirements. The repeated inherited suite completed **356
passed, 5 failed, 59 skipped** in 312 seconds; the five failures have the same known baseline symptoms
recorded above. No inherited tests were changed. Linux and CI results have not been observed passing.

The [session implementation boundary](vr-renderer-session.md) describes ownership and stall limits.
Next add a timed game-camera consumer and actual per-eye images with correct projection/culling/
temporal state; the independent diagnostic frame loop alone does not align game and XR frames.
Logs, captures, generated game code, saves and binaries remain local under ignored work folders.

## Offline camera/projection follow-up — 2026-11-12

The user was remote without headset access, so no real XR session was opened in this stage. New
camera observations are explicit opt-in and read-only. The renderer table gained one callback and
was bumped from ABI 17 to **18**; both experimental host and renderer were rebuilt. No instruction
hook or translated game code was changed. All 80 translated compilation units kept their earlier
cache keys. The final paired build completed native compilation in 66 seconds with zero failed
units and linked successfully. Both private runtime renderer paths have identical DLL hashes.

A bounded 180-second headless smoke with a fresh hash-verified copy of stable saves loaded Dodongo's
Cavern. The last logged aggregate reported **3,204 primary-camera observations, 19,296 uniform-view
observations, 9,402 matrix matches and zero rejected packets**. These are diagnostic observations,
including repeated jitter sites, not unique frames or proof of main-scene GPU draw ownership. The
smoke peaked at 6,608 MiB sampled working set. No guest matrices were modified by the observation.
The fixture also checks the actual host observer helpers with authored memory and verifies that it
remains unchanged. Default-off observation leaves the existing jitter outputs/locking behavior intact.

An initial inherited-suite run exposed the fixture's missing declaration for the new observer call.
The fixture now extracts the real observer helpers and adds a separate opt-in/read-only case without
weakening any jitter assertions. The expanded fixture passes for both the current source and the
unchanged upstream archive. A repeated full inherited suite completed **356 passed, 5 known baseline
failures, 59 skipped** in 301 seconds. The 5 failures are the same previously classified tool/.NET/
symlink/fixture issues; this remains a limited suite, not a fully green baseline.

All **34 native VR CTest cases pass**, including the new C/C++ ABI layout/transport, capture
correlation, per-eye FOV/projection/depth conventions and unsubmitted stereo-plan coherence checks.
After the final conservative invalid-primary clearing adjustment and rebuild, a 20-second flat
smoke with the final experimental renderer used Vulkan without XR log markers and peaked at
3,027 MiB. It ended at the requested time limit and left no game process active. The 19 inventoried
stable files retained their SHA-256 baseline hashes.

The game projection uses NVN -1..1 depth, with conversion in the existing shader path. Both depth
conventions, both depth orders, vertical flip and finite/infinite far planes now have synthetic
projection tests. None of this implements headset game images. The diagnostic stereo-plan builder
is pure and not connected to live camera/pose data. Actual timed camera consumption, per-eye target
rendering, culling, temporal state, body/collision scale and visual comfort remain pending. See
[offline camera work](vr-offline-camera.md). No raw observations, captures, saves, binaries or generated
game source were published; all remain under ignored private work directories.

## Flat-quad transport preparation (2026-11-12)

The experimental renderer now offers a bounded game-image quad transport. This is
an intermediate flat screen in XR space, not native stereo gameplay. No real XR
session was opened during the remote/offline development of this feature.

A 120-second real-game readback-only test, with a private save snapshot, reported
1,860 captured 1280x720 RGBA images by its last diagnostic sample and reached a 3D
scene. It peaked at 6,562 MiB sampled working set. No PNG frames were written.
This validates GPU readback and CPU feed delivery, not actual headset upload,
compositor visibility or VR performance. Real VDXR visual testing remains pending.

All 45 VR CTest cases pass, including quad uploads, reuse without a second upload,
noncoherent flushing, unsupported formats, allocation/submit/wait failures, invalid
image indices, unresolved-GPU resource retention, feed expiry, update-worktree
conflicts and build compatibility guards. The inherited suite completed 357 passed,
5 known baseline failures and 59 skipped. The same baseline limitations remain:
fixture header/tool dependencies, missing .NET reference packs, Windows symlink
privilege and an existing unused-variable fixture warning. Capture visibility tests
also passed their 16 ownership/coherency combinations.

Source update tooling and synthetic Git fixtures prepare isolated candidates and
retain conflicts without promoting compatibility. The launch guards reject unknown
game build IDs, ABI mismatch, modified binaries and stale source fingerprints.
See [preview procedure](vr-headset-preview.md) and [upstream maintenance](vr-upstream-updates.md).

## Confirmed Quest transport (2026-11-12)

The user confirmed the game image, normal colors, a world-anchored screen and a
working monitor in the bounded VDXR quad preview. It submitted 7,716 quad layers
using 1,679 game images; all 8,165 session frames had valid head and stereo poses.
The session closed with no reported XR API errors; peak sampled working set was
6,717 MiB. All 19 inventoried stable hashes remained unchanged and no game process
was left running. Raw logs and the visual confirmation remain private under work/.
This validates flat image transport only. The original test deliberately had audio
disabled. The preview launcher now opts into stereo audio and supports `-Mute`;
audio routing/audibility in the headset has not yet been confirmed.

## Submitted scene-camera evidence and audio (2026-11-12)

The rebuilt paired binaries compiled in 103 seconds with zero failed units. All 46
VR CTest cases passed. The required inherited suite again reported 357 passed,
5 previously classified baseline failures and 59 skipped; it is not fully green.

A 150-second flat diagnostic with private save data reached Dodongo's Cavern and
peaked at 6,609 MiB sampled working set. The last aggregate sample reported
2,180,000 HDR/depth draw candidates: 56,543 invalid blocks, 1,540,424 without a fresh
host association, 86,972 view mismatches, zero projection mismatches and 496,061
matches across 740 matched replay-frame transitions. These are candidate observations,
not counts of completed GPU draws or proof of unique main-camera ownership. The high
missing-association count is a remaining synchronization limitation; the current
latest-only host mailbox is insufficient to authorize a reliable camera override.
No game matrix, shader, simulation or GPU replay behavior was changed.

The audio renderer opened stereo output on the Windows default device. This proves
local device opening only; audible Quest routing still requires user confirmation.
No OpenXR session was opened by this diagnostic. The new launcher result guard
recognizes the saved positive camera evidence and fails a requested scene-capture run
when such evidence is absent. Native stereo game rendering and head-driven gameplay
remain unimplemented. See [scene-camera provenance](vr-scene-camera.md).

## Submit-time camera snapshot validation (2026-11-12)

All 47 VR CTest cases passed. The new test exercises the actual camera callback and
thread-local scope implementation as well as bounded history/store, future/stale
rejection, camera identity ambiguity, equivalent stationary samples, nested scopes,
unknown-submission rejection, resource lifetime across eviction and context refresh
without mutating work already active on another thread.

A 150-second muted flat run with a fresh private save reached Dodongo's Cavern. Short
right-stick camera turns were sent through the game's private test-input interface.
At the last aggregate sample, both methods examined 1,996,000 HDR/depth candidates:
submit-time matching accepted 1,522,485 (76.3%), versus 610,217 (30.6%) using only the
latest host association. No candidate lacked a submit context; 3,583 had ambiguous
camera identities and were rejected. Many accepted candidates still had equivalent
host observations: identical matrices do not identify a unique simulation tick.
Peak sampled working set was 6,663 MiB. Both positive-evidence runner gates passed.
These counts describe candidate inspection, not completed GPU draws or verified
stereo rendering. No OpenXR session was opened, and no camera matrix was overwritten.

The full inherited run initially completed 356 passed, 6 failed, 59 skipped. One
additional failure was the timeline fixture's unknown context argument. The fixture
was updated without removing allocator/order assertions, with an extra check that
camera binding reserves no timeline values. It then passed against both the modified
source and the original upstream source. The other five are the previously recorded
baseline failures. The full suite was not repeated after this fixture-only correction.
The update-worktree fixtures also passed after their source allowlist was extended.

## Resource lineage, live host refresh and matrix audit (2026-11-12)

See [resource lineage](vr-image-lineage.md) and [private camera audit](vr-camera-uniform-audit.md)
for the measured captures, limits and field candidates. All 53 VR CTest cases pass.
The inherited suite remains 357 passed, 5 classified baseline failures, 59 skipped.
The upscale fallback fixture retains every prior behavior assertion and passes its
23 cases against both this source and unchanged upstream. No real XR session was
opened by these diagnostics. Stereo game images and camera writes are still absent.

## Shader binding audit (2026-11-12)

All 57 VR CTest cases passed, including private bounded module export and static
Uniform access analysis. The inherited suite again completed 357 passed, 5 known
baseline failures and 59 skipped. Cache-cleanup fixtures retained their existing
module lifetime assertions and additionally checked observer notifications against
successful module creation.

A 150-second headless private-save run exported 581 modules locally, below the
64 MiB cap, and recorded 64 representative emitted camera programs. Its 99 checked
camera bindings yielded 82 unique queries, 80 resolved static subsets and 2
unresolved subsets, with no missing primary modules or parse errors. Fragment
bindings read the inverse-view region, while other accesses extend beyond the
small captured prefix. Peak sampled working set was 6653 MiB. No shader, uniform,
Vulkan pipeline input or guest memory was rewritten. No XR session was opened.
See [shader consumption](vr-shader-uniform-audit.md) for the scope and remaining gaps.

## Private eye-uniform preparation (2026-11-12)

`vr/src/eye_uniform_copy.h` now builds an unsubmitted CPU copy of the complete
caller-supplied descriptor payload. It validates the original rigid view, VP=P*V
and inverse view, validates the target eye matrices and eye position, and changes
only the reviewed view/VP/projection/inverse-view region at 0..0xe0. All other
bytes remain exact, including unknown values at 0x1c0 and beyond 0x230. Payloads
are bounded to 1 MiB; truncated, aliased, nonfinite, inconsistent and zero-to-one
clip inputs fail without replacing the previous output. The caller must still
establish the reviewed game profile, full descriptor extent and NVN clip convention.
This helper neither rewrites a live game buffer nor submits GPU work.

The new authored test exercises asymmetric projection, pitch/yaw/translation,
distinct eyes, inverse composition, complete-payload preservation and rejection
paths. All 58 game-free VR CTest cases pass. No additional headset test is needed.
The final default-off isolated game smoke completed its 20-second bound at
2971 MiB peak working set, with every optional diagnostic/session flag off.

## Native owned-eye resource smoke (2026-11-12)

Added a standalone, one-shot Vulkan resource owner with separate eye images,
buffers and dedicated allocations, bounded actual requirements, failure cleanup
and completion-fence lifetime. All 59 VR CTest cases pass, including 13 injected
creation/allocation/bind/fence failures and pending/device-loss retention.
The explicit native smoke on RTX 2060 SUPER passed all 2048 image pixels and
2048 buffer bytes, with 10,240 resource-allocation bytes total for both eyes plus
staging, and a completed fence. No game or XR session was opened. This validates
ownership/transfers only; full-scene allocation/remapping/replay is still pending.
See [owned-eye resource scope](vr-eye-vulkan-resources.md).

Commit 88080734 also reapplied cleanly onto the recorded upstream base in an
isolated source-only worktree. This does not certify compatibility with later
upstream versions or enable release readiness.
The native smoke additionally uploads two authored full eye-camera uniform payloads
and verifies all 2048 returned bytes. The original CPU payload remains unchanged,
including unknown fields outside the reviewed matrix region. This is synthetic
input, not a measured game calibration and not proof of shader-wide patch safety.

## Projection depth, reusable resources and compute audit (2026-11-12)

The pure `projection_depth.h` helper recovers near/far endpoints and forward or
reversed order only for the independently reviewed NVN -1..1 path. It handles an
infinite or float-unresolved far plane explicitly; coefficients alone cannot infer
the clip convention or world units. Authored tests include asymmetric/Y-flipped
projections, reversed/infinite depth, float far-plane rounding and invalid inputs.
No live camera calibration is silently created.

The reusable resource smoke passes two real GPU submissions without new eye
allocations: 4096 pixels, 4096 full-payload buffer bytes, both fences complete.
The fake-Vulkan test covers eight reuse cycles and failed-reset guards.

The first compute-only 150-second private-save run observed 31 stage-5 programs,
56 user-UBO bindings, no missing modules, a matched scene/source camera and no
canonical primary-camera block binding. Peak working set was 6641 MiB. This is not
camera-independence proof. A wider submitted-matrix candidate search is now under
validation; it reports candidate offsets without enabling semantic patch safety.

## Same-process compute/resource candidate plan (2026-11-12)

All 62 VR CTest cases pass. The final full inherited suite remains 357 passed,
five documented baseline failures and 59 skipped (301 seconds, two jobs).
Native renderer build completes in 71 seconds with zero failed units.

A combined 150-second private-save compute/lineage run completes at 6515 MiB peak
working set, with matched scene/source cameras and a verified incomplete graph.
Same-process association hashes verify. Seventeen compute-camera programs are
observed, 16 occur in the representative frame; conservative propagation adds
10 seed events and 19 dependent events in the present slice. Per-eye work becomes
847 graphics/36 compute events (baseline 847/17). Buffer references increase to
81,242,064 bytes per eye; image allocation references remain 78,084,768 bytes per
eye. The combined two-eye reference is 318,653,664 bytes, about 304 MiB, excluding
alignment, resolution changes and all untracked/additional resources. No real
scene eye allocation, replay, camera override or XR projection layer is enabled.

The final default-off isolated smoke completes its 20-second bound at 3069 MiB
peak working set, with all capture/session flags off. The 19-file stable-install
SHA-256 baseline remains unchanged; no diagnostic game process is left running.

## Native game-image clones (2026-11-12)

Added a source-shape-aware pair factory and one-attempt dedicated/device-local VMA
entry point, with preflight requirements/budgets and no host/pool/over-budget
fallback. Depth/stencil copies split aspects, full mips/layers are retained, and
the owner retires only after its actual last-use timeline value. The optional game
probe receives the exact draw recorder value, keeps source Texture objects and
handles intact, and polls completion once per later frame.

All 64 VR CTest cases pass, including an extracted actual strict-allocator test.
The final inherited suite is 357 passed, five documented baseline failures and
59 skipped in 296 seconds. Native paired build passes in 69 seconds, zero failed
units. A 150-second private-save/headless/muted/no-XR probe passes with matched
scene/source cameras, GPU copy completion and resource release. The sampled
1152x648 HDR color/depth copies use 14,155,776 actual allocation bytes (13.5 MiB)
for four private images; peak process working set is 6591 MiB. No source image
object, descriptor or guest registration is replaced. No stereo replay is enabled.
Raw result/probe/log stay private under ignored work/validation.

The final 20-second default-off smoke completes at 3040 MiB peak working set,
with the eye-image probe/captures/XR disabled. The stable 19-file SHA-256 baseline
is unchanged and no diagnostic game process remains running.

## Private native image views (2026-11-12)

The clone owner now supports bounded pairs of exact-format 2D/2D-array views,
with checked source identity/mip/layer/aspect ranges, atomic outputs, partial-view
failure cleanup and a 32-pair cap. Views are destroyed before image allocations
under the same last-use timeline. The probe uses active Targets format/mip/layer
metadata and verifies creation/release of four private views without installing
them in original descriptors or targets.

All 64 VR CTest cases pass, with added view failure/cap/order checks. Full inherited
suite remains 357 passed, five known baseline failures, 59 skipped in 305 seconds.
Native build passes in 72 seconds. A 150-second isolated no-XR probe passes with
four views created/released and all image-copy completion/retirement checks true;
image allocations total 14,155,776 bytes, peak working set 6578 MiB. Raw probe/log
and result remain private. Texture/loader contracts now participate in upstream
review and CI; their two maintenance checks pass after the manifest change.

The final 20-second default-off view-build smoke completes at 3064 MiB peak
working set. Probe/capture/session flags are off, the 19 stable-install file
hashes remain unchanged and no diagnostic game process remains running.
