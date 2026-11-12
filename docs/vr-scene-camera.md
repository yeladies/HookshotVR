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
Simulation and writes into shared original resources must remain single-execution.
Camera-dependent compute may repeat only into fully reviewed private eye resources;
the current diagnostics do not establish that isolation. Actual XR poses must be
latched coherently with the submission before enabling a head-driven camera consumer.

## Submit-time camera context

`xr_camera_submission.h` adds a bounded history of 16 validated camera candidates.
The submit path copies candidates received in the preceding 250 ms **before** taking
the submission's memory capture. The source NVN submission value, not a combined
Vulkan burst/timeline tag, keys a bounded 256-entry store. Both immediate and queued
submissions carry the snapshot; a thread-local RAII scope selects it each time
`run_submit` starts or resumes. Paused queues keep their own original associations.
When a CPU event causes a successful fresh memory capture on resume, its camera
snapshot is refreshed at that same capture boundary. An unknown/evicted context
never falls back to the newest host camera.

Matching compares the submitted block to the pinned candidates. A snapshot more
than 500 ms old at replay fails closed. Candidates from after the capture boundary
are excluded. Multiple equivalent observations from the same camera remain counted
explicitly; matching different camera identities is ambiguous and rejected. A paused
or stationary scene cannot establish a unique simulation tick merely by matching a
matrix. Camera and memory capture are adjacent operations, not a transaction across
all guest writers. This stage remains diagnostic and does not authorize overriding
camera matrices or duplicating GPU writes.

`vr-submit-camera` counters allow comparison against the earlier latest-only
`vr-scene` counters in the same run. `scene_submission_matched` is the runner's new
positive-evidence gate. The diagnostic wrapper disables parallel segment recording
and the separate recorder: workers without an explicitly supplied context remain
unassociated. Async queue support preserves the submit-time snapshot, but has not
yet had a live-game validation. Benchmark replays do not synthesize a fresh context.
All future per-eye work must respect these unsupported paths until implemented.

The [frame resource-lineage observer](vr-image-lineage.md) now follows emitted commands
from the matched camera toward the image presented. It remains a conservative,
incomplete diagnostic and never enables automatic stereo replay.
