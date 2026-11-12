# Frame resource lineage toward native stereo

`run-game.ps1 -ImageLineage` implies the read-only camera diagnostics, captures only
emitted graphics/compute commands, and records a bounded private frame graph. It
opens no OpenXR session and does not duplicate GPU work. The observer names the
renderer-created image IDs and native storage-buffer ranges, never copies shader
code or guest data into the graph. Its JSON stays under ignored `work/game-run`.

Each write creates a new resource version; reads point to earlier versions. Whole
image identity is deliberately conservative across mip/layer/aspect ranges. Blending
and partial writes retain prior destination dependencies. The camera seed is accepted
only from the submit-time camera matcher when a graphics command is actually emitted.
Descriptor reuse carries the corresponding observed image/buffer identities forward.
Storage ranges use native buffer identity, offset and size; overlap is checked without
integer wraparound, so unrelated ranges of the same imported pool need not share
camera dependence. Hidden pointers, texel buffers, indirect/vertex/index reads, copies,
clears, temporal state and per-eye culling still need additional coverage.

The observer tracks at most 4096 commands, 2048 image identities, 64 declared resource
bindings per command and bounded storage-buffer versions/dependencies. Overflow
invalidates the capture rather than silently authorizing a partial replay. A
representative seeded frame connected to the presented image is saved after startup.
Both `coverage_complete` and `replay_ready` remain false, including successful tests.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 150 -SnapshotStableSaves -LoadTestSave -ImageLineage
python -I -B vr/analyze_lineage.py work/game-run/image-lineage.json
```

The Python analyzer follows image and observed buffer producers backwards from the
presented image. It rejects unknown/future or duplicate versions, and reports shared
versus camera-dependent work candidates and private resource candidates. This is a
review tool, not a replay scheduler. CPU simulation, shared writes and GPU readbacks
must keep their original single-execution meaning. Actual eye resources, descriptor
remapping, per-eye temporal history and calibrated XR camera writes remain pending.

The first image-only live capture reached Dodongo's Cavern: 1540 commands observed,
905 in the conservative present slice (892 graphics and 13 compute), 683 camera seeds,
831 camera-dependent commands, 36 private-image candidates and 60 imported-image
roots. Of the dependent commands, 822 were graphics and 9 compute. There were 463
slice commands with unresolved buffer accesses. A 150-second run peaked at 6646 MiB.
These are one frame's observations, not a measured stereo performance budget. The
following buffer-range extension is being validated separately.

The original upscale-fallback fixture needed a declaration for the observation
callback. Its existing format, warning, write and fallback assertions were retained;
all 23 cases now also check exactly one input/output-pair notification on success.
The fixture remains usable on unmodified upstream source.

## Buffer-range extension and address layout

A following 150-second private-save run passed the image-lineage analyzer and peaked
at 6565 MiB. Its representative frame contained 1137 observed commands, 953 in the
conservative present slice (887 graphics, 66 compute), 679 camera seeds and 845
camera-dependent commands. The observed range dependencies included 150 storage
writes, 278 imported buffer ranges, five private-written range candidates and 49
private-image candidates. These are different frames from the image-only capture;
the counts are not a direct performance comparison.

Closing the five private writes over overlapping descriptor read ranges produced
five alias groups totaling 69,646,032 logical bytes per eye in that frame. This is
not an actual Vulkan allocation or a complete memory budget. The pure
`EyeBufferLayout` maps each eye to a separate allocation index, preserves original
descriptor offset alignment with a prefix, rejects partially covered descriptors and
refuses unplanned per-eye writes to original storage. Actual Vulkan allocation,
initial copies, descriptor/address remapping and retirement fences are still pending.

A later inventory run initially saved no frame because its startup filter required
30 seeded frames although four valid connected frames were observed. This exposed a
publication-order/freshness problem in camera matching, now addressed by the guarded
[submit-time host refresh](vr-camera-uniform-audit.md). The later refresh test below completed live validation. The image inventory includes dimensions, levels, layers,
samples, format/aspect/type, sparse status and a conservative resident-allocation
reference, to make the eventual private-image budget reviewable.

## Validated inventory after host refresh

The following checked-at-submit refresh test produced a valid representative frame
with all 49 private-image candidates described. Their observed resident-allocation
reference totaled 77,566,368 bytes per eye (155,132,736 for two), with no missing,
zero-size or sparse-image entries. Together with the 69,646,032-byte logical buffer
closure per eye, the two-eye reference is about 281 MiB. This excludes alignment,
additional eye/temporal targets, aliases not observed and any change in render size;
it is not an actual allocation or a prediction of stereo frame rate. No private
Vulkan resource has been allocated or used by this prototype yet.

The full inherited suite with the range/inventory changes again reported 357 passed,
5 known baseline failures and 59 skipped. The expanded VR suite has 53 passing CTest
cases including range overlap, inverse-matrix analysis, guarded C host reads and
per-eye address-layout isolation. Actual Vulkan factories, copies, bindings,
retirement fences and complete shader-field consumption remain the next gates.
