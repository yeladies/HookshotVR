# Live depth-reconstructed preview

The moving diagnostic keeps the original game running in its isolated private-save
copy and reconstructs successive color/depth frames in the separate viewer. It is
not full native stereo game rendering. The frozen build and pending headset check
remain available separately. No XR session was opened during development/testing of
this mode while the user was remote.

## Run locally

Connect Quest 3 through Virtual Desktop, then run the prepared private
`work/headset-live-preview/Probar-en-movimiento.cmd`. The source launcher is
`vr/run-moving-preview.ps1 -Seconds 60`. Allow about 90 seconds for preparation and
save loading; the viewer then opens for 60 seconds. Both processes are bounded and
closed afterward. A stop signal terminates the game runner; forced cleanup checks
its recorded executable path and exact start time before stopping a native PID.
The stable installation and its saves are never the runtime write target.

Use `-Offscreen -Seconds 15` for the same producer/consumer chain without creating
an XR instance/session. Its GPU observer records two eye images and compares temporal
changes with fixed camera poses. This is the automated motion check, not a headset
perception or FPS check. The source camera follows the game's camera; head poses are
applied in the independent viewer. VR controls/audio and full native camera/culling
integration remain outstanding. Hidden surfaces, transparency and disocclusion holes
cannot be recovered from a single source depth view.

## Transport and resource ownership

`-DepthSceneStream` implies the matched depth capture mode in `vr/run-game.ps1`.
The producer publishes at most 300 frames, with a native minimum interval of 100 ms
and a default of 200 ms. Only a fresh output path inside project/work is accepted.
Each complete V2 packet contains the same-frame color, D32 depth, projection/view
and actual sampled color format. Atomic replacement prevents partial-frame reads;
publication retries retain the completed data instead of recording another copy.
Only the latest frame and one temporary file are kept by the transport.

One cached mapped readback is reused after known GPU completion. It grows only if
necessary, remains capped by the existing 16 MiB actual-allocation guard, and is
released on stream completion. Unknown/device-changed completion retains submitted
storage. The capture reads the source images without patching shaders/uniforms or repeating
game updates.
The cached allocator already exists in the upstream renderer; no ABI table changed.

The viewer accepts only complete bounded packets with valid projection and rigid
view matrices. A single persistent 16 MiB storage buffer receives the newest frame;
it is updated only after both eyes of the previous frame have completed. Both eyes
use the same accepted capture. An authored vertex shader reconstructs the grid on
GPU, rejects invalid depth/discontinuities, levels the source orientation and applies
per-eye projection/pose. No CPU triangle-mesh rebuild is needed. Stale streams stop
submitting a scene layer after two seconds; the viewer never follows a partially
written or mixed color/depth packet.

## Measured checks on 2026-11-13

Windows RTX 2060 SUPER, 16 GB RAM, capture fingerprint `d0fbac6a`, ABI18:

- The optimized 15-second real-game check rendered 67 updated stereo pairs, about
  4.5 capture updates/s, and accumulated 14937386 temporal pixel differences.
- The source published 70 frames using exactly one 5971968-byte readback. Maximum
  measured CPU publication time was 12101 microseconds. Peak game working set was
  6612 MiB. The stop signal closed the producer with `stop_requested`.
- At two 800x600 offscreen views, mean command-recording/GPU-completion/copy time was
  1458 microseconds per pair; verification readback/file time was 7550 microseconds.
  These are diagnostic timings, not headset frame rates or motion-to-photon latency.
- A separate synthetic GPU test held color constant while changing only depth:
  45 accepted frames, zero color-change frames, 44 depth-change frames and 1659573
  changed output pixels. It proves dynamic geometry routing independently of game
  texture animation. Run `vr/tests/run_depth_motion_gpu.py --viewer EXE` explicitly;
  this hardware test is not a CI requirement and reads no game data.
- All 78 VR tests pass. The default-off startup and 19 stable installation hashes
  were checked. The inherited regression result is recorded in the development
  checkpoint. The new mode's headset perception, scale and performance are pending.

All packets, raw views, game-derived meshes/images, binaries and session folders stay
private under work/. Public artifacts are authored source, shaders, tests and docs.
The native full-scene eye renderer remains a separate development path; this preview
does not permit repeating the game loop or relaxing the isolated-draw replay guards.
