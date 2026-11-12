# Bounded Quest game-image preview

This is an intermediate transport test: the ordinary flat game image appears on a
world-anchored screen in OpenXR. It does not implement per-eye game rendering,
head-driven gameplay cameras, room-scale collision or tracked weapons.

## Run when the headset is available

Connect Quest 3 through Virtual Desktop, select the already configured VDXR runtime,
and wear the headset. From this repository run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/Start-Quest-Preview.ps1 -Seconds 120
```

The launcher uses matched experimental binaries, checks the game build ID, renderer
ABI, source fingerprint and binary hashes, then takes a private save snapshot.
It attempts scripted menu/save loading; cold shader loading may delay that sequence.
A menu image is still useful for the transport test. The preview enables stereo audio on the Windows default output (normally redirected
by Virtual Desktop); `-Mute` disables it. It uses a headless
mirror and closes automatically. Nothing is installed into the stable game directory.
Do not run it until both experimental build receipts are current.

Check that the image appears upright, with readable text, correct colors and no
left/right inversion. Move the head gently: the screen should remain anchored in
space. The scene inside the screen still uses the ordinary game camera. Check the
monitor remains normal and record any runtime errors. VDXR visual behavior was confirmed by the user on 2026-11-12: visible image,
normal colors, world anchoring and a functioning monitor. Audio was disabled in
that original test; audible headset output still needs checking.

## Implementation and limits

`vk_replay.cpp` shares the existing screenshot readback ownership/timeline path.
Preview capture runs every second native present at 1280x720 RGBA, without writing
PNG files. A bounded mailbox owns immutable CPU pixels; images expire after two
seconds. The XR owner uploads new images through a staging buffer, flushes
noncoherent memory, submits under the shared Vulkan queue lock and waits for the
GPU fence before releasing the swapchain image. The same released image is reused
between game updates. OpenXR composition uses the most recently released image:
see the [OpenXR rendering specification](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/rendering.adoc).

The quad is 2.4 by 1.35 meters, initially two meters ahead at head height with yaw
alignment. Reference-origin changes re-anchor it. Upload timeouts stop submission;
resources still referenced by unresolved GPU work remain alive until process exit.
The readback and upload route is intentionally a diagnostic bridge, not the final
low-latency stereo renderer. Its cost must not be used to predict native VR speed.

For offline GPU-to-CPU validation without creating an OpenXR session:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 30 -PreviewReadbackTest
```

A successful offline test proves image capture and feed delivery, not compositor
submission or headset visibility. Fake-runtime tests cover upload, repeated-frame
reuse, queue locking, cleanup and injected failure paths. The next native VR stage
must identify main-scene draw ownership and supply independent per-eye targets,
culling and temporal histories from one simulation tick.
