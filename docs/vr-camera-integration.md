# Camera integration boundary

`vr/src/camera_math.h` is a game-independent, tested conversion from a valid OpenXR pose to a
row-major, right-handed world-to-eye 3x4 affine matrix. It retains head roll and positional tracking.
A caller supplies the world anchor, tracking-space reference position, body yaw and world units per
metre. The default unit scale is a mathematical identity, not an established game calibration.
Input quaternions must be near unit length and are normalized before constructing the basis.
Invalid or nonfinite samples return false without changing the output. Callers must honor that
return value; preserving output does not authorize submitting a stale pose.

An `XrView.pose` already includes the runtime eye offset. Convert each eye directly with the same
origin; do not add a second IPD offset or rotate it by head orientation again. The helper accepts
position/orientation validity flags; supplying those flags and a pose from the same predicted frame
is the caller's responsibility. It does not own an XR session, frame timing, recenter event, player
collision or per-eye projection. It is not yet called by the game or standalone marker renderer.

## Existing game hook

At the audited baseline, `nx2/oot_patches.py:patch_view` inserts `hos_oot_free_view` after the guarded
matrix construction site. `host/hos_main.c:oot_gameplay_view` selects camera zero and the hook checks
the known view vtable before overriding anything. The existing debug camera writes three float rows
to `view + 8`, with axis dot eye negated in the fourth column. Its eye/target inputs are doubles at
`view + 96` and `view + 120`. The new helper's row layout matches that debug view convention, but this
has not been validated by writing a tracked pose in a running game.

The debug hook alone is insufficient for native stereo: it does not identify all culling inputs,
projection paths, camera-dependent effects or player head/body state. Never enable the debug free
camera and an XR override simultaneously. All game pointer, supported-build and camera guards must
remain in place. Do not substitute headset motion with mouse or right-stick deltas.

## Next implementation gates

1. Capture an immutable gameplay camera snapshot at the guarded hook, including world origin and
   projection provenance. Identify the main-scene uniform update versus UI, reflections and movies.
2. Refactor standalone session/device negotiation into a renderer-owned opt-in XR lifecycle; use
   the runtime-selected GPU and required Vulkan features/extensions before device creation.
3. Publish pose samples with explicit frame identity, predicted display time, validity and tracking
   origin generation. Synchronize host/renderer exchange; bump `MKWVK_API_VERSION` if its ABI changes.
4. Build per-eye FOV projections using the actual renderer's depth/Y conventions and independent
   temporal histories. Confirm eye pose and projection correspond to the same predicted time.
5. Render the camera-dependent graphics for two targets while preserving one simulation tick,
   submit-time memory captures and cross-queue waits. Do not repeat compute writes or physics work.
6. Verify stable landmarks under translation, yaw, pitch and roll; world scale, culling, near plane,
   HUD and camera transitions need live tests. Room-scale collision and physical weapons follow.

## Checks

`vr_camera_math` checks identity, eye-to-origin mapping, recenter translation, explicit scale,
body yaw, head roll, runtime eye separation/parallax direction and invalid pose rejection. It uses
synthetic inputs only. Together with `vr_probe_contract` and `vr_scene_math`, the three native tests
passed locally on Windows. They do not establish working in-game VR or accurate physical scale.
The [renderer device negotiation](vr-renderer-openxr.md) part of gate 2 is implemented and locally
validated. Renderer-owned session/frame/pose integration and all per-eye game rendering gates remain pending.

A bounded [renderer session/pose probe](vr-renderer-session.md) now supplies internal snapshots
with predicted time and origin generation. No game camera consumer or stereo replay is connected yet.
