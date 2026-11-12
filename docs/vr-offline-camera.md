# Offline camera capture and per-eye projection work

This stage can be built and tested without a connected headset. It does not start OpenXR, write an
XR pose into the game, allocate eye swapchains or display stereo game images. It adds optional,
read-only observations to identify the main-camera path before changing its behavior.

## Versioned host/renderer transport

The callback `MkwVkRenderer.camera_observation` changes the table, so `MKWVK_API_VERSION` is now
**18**. Rebuild both experimental host and renderer; do not mix an ABI-17 binary with an ABI-18 one.
The Windows host object cache now includes `vk_api.h`, as the Linux host cache already did, so a
future table/header change invalidates the host objects that depend on it. The translated game
object keys remain unchanged in this stage; no new game-instruction patch or translation is needed.

With `HOOKSHOT_VR_CAMERA_CAPTURE=1`, the guarded gameplay-camera hook reads the existing view rows
and double-precision eye position before any debug free-camera override. It keeps the supported
vtable and camera-zero checks. The existing guarded temporal-jitter hook also observes the uniform
view and projection before jitter is added. The observation does not change the jitter result.
All packets are copied across the table, not retained as pointers to mutable game memory.

These are **different objects**: the camera's matrix starts at its +8, while the renderer uniform
view's matrix starts at its +0 and its projection at +0x1a8. Guest IDs stay opaque provenance; they
are not assumed equal or dereferenced in the renderer. The receiver checks packet size, monotonic
sequence, finite rigid view matrices, source flags and perspective-matrix shape. Debug-camera
observations are excluded. It searches for matching matrices with the same observed host-present
count and within 100 ms, retaining a copied candidate pair. This is a diagnostic association, not
proof of unique main-scene draw ownership, an identical simulation tick or XR frame alignment.
Auxiliary views can have the same matrix, and the three jitter sites can produce duplicate observations.

Default startup clears the opt-in. The host adds no extra uniform-view memory read or callback
when observation is off. Observations never write guest matrices, options or saves. Logs contain
aggregate counters only; raw matrices/guest IDs are not published.

## Per-eye matrix builder

`vr/src/projection_math.h` builds row-major, column-vector, right-handed projections looking along
-Z, directly from asymmetric FOV angles. Clip depth is explicitly selectable as -1..1 or 0..1,
as are forward/reversed depth, vertical flip and a finite/infinite far plane. Invalid input leaves
the output unchanged. There is no silent clamping of an invalid FOV or invented world scale.

The current game's projections use NVN's -1..1 convention. Existing renderer code performs its
conversion to Vulkan 0..1 (`vk_shader.cpp` and the SPIR-V depth-mode path); temporal code also reads
the original OpenGL-style projection. Supplying a 0..1 projection to that same converting path would
apply the conversion twice. The tested builder supports both conventions, but a future consumer
must choose the one appropriate to its exact insertion point. Y inversion likewise remains explicit.
No generated shader headers or shader behavior were changed in this stage.

Tests cover asymmetric frustum edges, near/far mapping for both conventions and depth orders,
vertical flip, infinite-far limits, stereo parallax direction, invalid inputs and equivalence of
exactly one NVN-to-Vulkan remap. Capture tests cover source identity, stale/different-present
candidates, matrix mismatch, invalid/debug packets, ordering and C/C++ payload compatibility.
The temporal-jitter fixture now includes the actual production observer helpers and verifies their
opt-in, offsets, copied fields and absence of game-memory writes while retaining all existing jitter
concurrency assertions.

## Unsubmitted stereo plan

`vr/src/stereo_plan.h` composes the tested view and projection math into an atomic pair of per-eye
view/projection/world-to-clip matrices. It retains game-camera provenance, XR sequence/predicted time
and origin generation. A caller must supply explicit calibration, same-clock receive timestamps,
near/far values and the appropriate clip/Y conventions. Camera or origin changes, stale/future/
widely separated observations, shouldRender false or one invalid eye reject the entire plan without
partially publishing one eye. Close timestamps are a guard, not proof that game and XR share a tick.

This is a pure diagnostic builder using authored synthetic inputs in tests. It is not wired to the
live pose mailbox or game-camera receiver, does not apply a head/body mapping, and submits no graphics.
It makes the next integration reviewable without needing the headset or inventing physical calibration.
## Local flat diagnostics

```powershell
# Rebuild the paired experimental host/renderer using an existing lift:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -SkipLift
# No -XrNegotiation or -XrSessionProbe: this run is entirely flat.
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 180 -SnapshotStableSaves -CameraCapture
```

Navigate using the private input-live file after inspecting the current screenshot. The normal
runner resource bounds, private save snapshot and read-only stable RomFS rules apply. Capture
counters are observations, not unique frames, and the JSON never certifies gameplay automatically.
The default renderer built above can omit SDK support; separately use `build-renderer.ps1` with the
SDK path for later explicit XR tests. All artifacts remain under ignored work directories.

## Remaining visual work

The next consumer must connect a copied camera candidate to the actual submitted scene uniform
and latch it with the XR frame's predicted-time pose/FOV. A camera-observation match alone does not
locate every view-projection use, resolve culling, identify Link's eye height/body transform or permit
replaying GPU writes twice. Stereo targets, isolated temporal history and UI/cutscene handling remain
pending. Headset-dependent image/scale/comfort checks are deferred until the user returns.