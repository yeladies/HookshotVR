# HookshotVR roadmap and integration boundary

## Goal

First-person PCVR with stereo rendering, independent six-degree-of-freedom head tracking,
tracked controllers, comfortable UI and eventually physical aiming, sword and shield interaction.
These are development goals, not currently supported features.

## Baseline

Development branch starts from NotRexed/Hookshot commit
`940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`. The existing fork was a strict ancestor;
it was updated locally by fast-forward, without removing fork-specific work.
The installed game reports v2.1.0 with source prefix `633eb435`; matching that binary to
this source is not established. Never mix the experimental DLL with the stable executable.

The working installation is an input for read-only inspection. Development, caches, test runs
and future game builds must use separate directories. Do not run the installer/update command
against the stable installation, use hardlinks for mutable data, or point test saves there.

## Milestones

1. **Bootstrap (implemented):** standalone OpenXR capability probe and game-free contract tests.
   Actual runtime results are local evidence, not committed personal machine reports.
2. **Session and graphics negotiation (basic live test passed):** the standalone Vulkan marker
   scene displayed in Quest 3 and returned valid head/controller poses. Full Hookshot graphics
   feature compatibility and exhaustive fault handling remain pending. The production renderer now also
   negotiates its Vulkan device through OpenXR on explicit opt-in; see `vr-renderer-openxr.md`.
3. **Isolated flat baseline (build/menu/save-load validated):** the audited source compiled and reached
   the main menu and a 3D scene from a private save snapshot. Stable file hashes remain unchanged. Full gameplay and
   broader save compatibility remain pending; see `vr-game-build.md` and `vr-validation.md`. The build
   avoids duplicating RomFS and uses matched experimental executable/renderer binaries.
4. **Head-tracked stereo (camera math/session poses tested; game stereo pending):** drive the main gameplay view from an XR origin, independent head pose
   and per-eye pose/projection. See `vr-camera-integration.md`. Render both eyes from one simulation tick, with isolated targets.
5. **Player integration:** room-scale/body synchronization through real collision handling,
   calibrated world scale, child/adult eye heights, head/body separation and explicit transitions.
6. **Hands and interaction:** controller actions, hand/weapon poses and physical aiming; find the
   authoritative attack/block/projectile functions before enabling physical sword/shield combat.
   The user-supplied classic `oot-vr` reference is retained in `vr-reference-projects.md` for this stage.
7. **Comfort and optimization:** UI layers, camera transitions, independent temporal history per eye,
   measured CPU/GPU budgets and regression tests for ordinary flat play.

## Source investigation map

- `nx2/oot_patches.py` / `host/hos_main.c`: main view hook (`patch_view`, `hos_oot_free_view`),
  gameplay camera selection and player physics controller. Existing free camera uses yaw/pitch;
  full quaternion orientation, roll and tracking origin still need implementation.
- `host/mouse_native.c`: game camera state and angle conventions; not a substitute for absolute XR poses.
- `nx2/oot_jitter.py`: validated instruction hooks around GPU projection uniforms. Preserve
  build guards and fail closed when expected code does not match.
- `gpu/vk/vk_main.cpp`: XR-aware Vulkan instance/device creation and runtime-selected physical device.
- `gpu/vk/vk_replay.cpp`: replay/present ordering, queue locks, resource lifetime and frame transport.
- `gpu/vk/vk_draw.cpp` / `vk_temporal.cpp`: scene camera/depth and some skinned draws. Bone buffer
  visibility is not yet an identified Link arm rig or authoritative gameplay collision interface.
- `host/nn_hid.c` / `hos_motion.h`: existing pad, gyro and haptics bridge; it lacks positional XR input.
- `host/mods.c`: RomFS overlays only, not a general binary VR plugin loader.

Preserve submit-time captures and cross-queue waits. Do not replay compute, physics-side writes,
queries or simulation twice just to obtain the second eye. A single final image copied to both
eyes is a virtual screen, not true stereo. CPU/GPU culling, reflections, HUD and temporal state
must be validated separately. Head motion must not be implemented as mouse deltas.

## Validation

Probe tests cover success, absent extension, unavailable headset, failed creation, absent stereo,
absent positional tracking, invalid eye dimensions, view enumeration failure, count growth and
stable output/exit semantics. They inject a fake API and do not claim real runtime validation.

Before integration changes, run the upstream game-free suite and document failures/skips.
Keep XR disabled by default in any future renderer integration. If the host/renderer ABI changes,
bump `MKWVK_API_VERSION` and rebuild both binaries. Store all game-dependent tests locally.

References: [Vulkan binding](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable2.html),
[frame submission](https://github.com/KhronosGroup/OpenXR-Guide/blob/main/chapters/frame_submission.md).

A bounded renderer-owned [session/pose probe](vr-renderer-session.md) has passed locally with zero
composition layers. It supplies internal predicted-time samples; game camera consumption and stereo
images are still pending. This does not upgrade the project to playable VR.
