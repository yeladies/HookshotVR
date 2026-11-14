# Status, evidence and roadmap

## Continuous native GPU eyes — 2026-11-14

The corrected isolated run completed/published 2,190 same-source GPU pairs at 1024×1024 per eye with synthetic camera motion, zero failures and no old preview image transport. Three output-only inspection samples change over time. A source-resolution-independent depth check removes the square-eye quantization stripes and passes a 49,152-pixel GPU oracle. All 114 VR cases pass; inherited baseline is 358 passed / 5 known failures / 59 skipped. Real swapchain/session consumption and HUD remain pending; this is not a validated headset build. See [continuous flow, how-to and limits](../../vr-continuous-gpu-eyes.md).

## Native live GPU pair — 2026-11-14

The normal isolated game test now reconstructs both eyes directly from the same live GPU source, with no source readback/upload: three pairs, 1152×648 per eye, 588,154 different eye pixels and zero reported failures. Completed output is read back only for inspection. All 110 VR cases pass. Current head-pose math is checked separately; OpenXR presentation, HUD and headset validation are still pending. See the [native path, how-to and limitations](../../vr-live-gpu-depth-pair.md).

## Current GPU source work — 2026-11-14

The native source pool has completed/published 300 immutable GPU color/depth packets with zero reported failures and 17,915,904 allocated bytes in an isolated game run. All 108 VR tests pass. A single packet was explicitly read back for inspection. The live pool is not yet connected to depth-pair reconstruction or OpenXR. The user has confirmed working audio in the later playable preview; its flicker, boundaries and HUD issues remain unresolved. See the [current source design and isolated how-to](../../vr-gpu-scene-source.md). Earlier dated observations below are historical.

[Home](index.md) · [How-to](howto.md) · [Status](status.md) · [Development](development.md) · [Help](help.md) · [English / Castellano](../es/status.md)

Status recorded **2026-11-13**, implementation reference `69f7b622f518d0618226f5ffc99464706f9bbecc`. Hardware: Windows, RTX 2060 SUPER, 16 GB RAM. Quest 3 transport: Virtual Desktop / VDXR.

## Verified evidence

- Cinema image displayed correctly in Quest. This does not verify the new depth viewers.
- Moving preview: 67 updated stereo pairs in a 15-second offscreen check, approximately **4.5 capture updates/s**. This is not headset FPS or motion-to-photon latency.
- Two 800×600 diagnostic eye images: mean render-pair measurement 1458 microseconds; verification readback/file work 7550 microseconds. These are separate diagnostic timings.
- Source capture used one reusable 5971968-byte readback; game peak working set approximately 6612 MiB.
- A synthetic GPU test changed depth while holding color fixed: 45 accepted frames, 44 depth changes and visible output movement. It requires no game.
- 78 VR tests passed. The inherited suite reported 357 passed, 5 known baseline/environment failures and 59 skipped. It is not fully green; details are in the [checkpoint](../../vr-development-checkpoint.md).
- Hashes of 19 stable installation files remained unchanged in the last validation.

No Linux runtime, AMD GPU, full-game compatibility, new depth-mode headset perception or headset performance claim is made. World scale is provisional. Audio in Quest remains unverified/unresolved.

## Incremental roadmap

1. **Pending human check:** frozen and moving depth modes in Quest; record scale, orientation, stereo, tracking and stability.
2. **Improve the diagnostic path:** capture cadence and memory/latency measurements; retain bounded resources and lifecycle handling. Single-view depth reconstruction will still have occlusion limitations.
3. **Native stereo:** complete per-eye resource ownership, camera aliases, culling, compute dependencies and temporal history. One simulation update must feed both eyes; do not repeat game logic per eye.
4. **Native 6DoF camera:** integrate head poses, world scale, recentering, camera transitions and first-person body visibility.
5. **Controls and mechanics:** locomotion, comfort options, tracked hands, aiming, weapons and physical interactions; preserve game progression and collision behavior.
6. **Release readiness:** gameplay and headset regression checks, performance budgets, compatibility profiles and reviewed update candidates.

The order is incremental, not a release schedule. [oot-vr](https://github.com/gibranfelix/oot-vr) is a research reference for future mechanics, not a dependency or proof that its implementation can be copied into this renderer. Review licenses before reusing code.

## Headset check update

The user subsequently confirmed a persistent moving depth scene, apparently correct scale and depth, and pixelation. The session accepted 254 source updates in 60 seconds and closed normally. Native stereo remains pending. See [native camera work and test limits](../../vr-native-camera-aliases.md).

## Remote eye-image inspection — 2026-11-13

A real-game offscreen run now saves bounded, private left/right eye samples. The source lens is 110° horizontal / 100° vertical with synthetic tracking; temporal eye images are distinct. One inventory image was captured, but continuous menu delivery still needs correction. Wide-view edge geometry needs culling review. This does not establish perceived stereo, comfort, audio or performance in Quest. No XR session is opened while the user is remote. See the [current experiment and capture instructions](../../vr-temporal-stereo-preview.md).

After the first wide/temporal headset comparison, [preview quality and continuity work](../../vr-preview-quality.md) adds opt-in HD mono, lens margin and bounded stereo-pair retention. Human validation remains pending; HD temporal stereo failed the actual freshness check and is not an accepted headset profile.
