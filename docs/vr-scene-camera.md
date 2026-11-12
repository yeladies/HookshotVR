# Submitted scene-camera provenance

The renderer now offers `-SceneCapture` in `vr/run-game.ps1`. This implies the existing
read-only host camera observations and adds an opt-in inspection of vertex uniform
binding 0 (NVN c3) at draw candidates with an HDR color target and depth attachment.
It does not override the camera, submit stereo images, or authorize a draw replay.

The copied 0x230-byte block comes from `ub_read`, which layers the recording's inline
uniform updates over `memory_read_as_submitted`. Reading current guest RAM here could
associate a later simulation update with an earlier draw. The diagnostic checks target
dimensions at 0x120, the affine view at 0, VP at 0x30 and projection at 0x70, using the
layout already consumed by the temporal path. It validates finite values, a rigid view,
perspective shape and VP = P * V before comparing to the fresh host camera candidate.
Only existing subpixel jitter at P02/P12 is tolerated separately.

Aggregate counters separate invalid blocks, missing fresh host associations, view or
projection mismatches, matched candidates and distinct replay-frame transitions with
a match. They publish no matrix values, guest addresses or program keys. A match is
evidence of a particular draw candidate's uniform contents. It does not prove unique
main-scene ownership, a completed GPU draw, exact simulation-tick alignment or an XR
frame association. Later draw filters may still skip a candidate, and auxiliary passes
may share the same camera. No Vulkan resource or guest memory is modified.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 150 -SnapshotStableSaves -LoadTestSave -SceneCapture
```

The next gate is to trace the matched camera through scene render targets and later
composites, then select camera-dependent work for independent eye targets/history.
Compute writes and simulation must remain single-execution. Actual XR poses must be
latched coherently with the submission before enabling a head-driven camera consumer.
