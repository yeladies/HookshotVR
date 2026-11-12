# Visibility and scene-replay integration gates

Native stereo requires more than updating a vertex camera uniform. Hookshot's
public source already exposes CPU and GPU visibility decisions that occur before
or outside a normal scene draw. These must remain explicit integration work.

## Evidence in the original source

- `nx2/oot_jitter.py` explains why CPU projection changes affect fitted shadow
  cascades and culling, while its existing uniform-only jitter avoids that effect.
- `gpu/vk/vk_draw.cpp` describes foliage task/mesh culling following buffer pointers,
  culling-table mirrors, indirect group counts and later passes consuming GPU writes.
- `RX_VK_NOCULL` changes Vulkan face culling. It does not recover objects discarded
  by CPU scene visibility, GPU instance/mesh culling, or zero indirect draw counts.

The current lineage records observed images and resolved storage-buffer ranges.
It does not yet prove complete hidden-pointer, vertex/index, indirect, copy, or
CPU-visibility coverage. The compute-only uniform audit checks the reviewed
primary camera block layout; a nonmatch cannot establish camera independence.
Specialized frustum planes, compact matrices or hidden-pointer payloads may use
other layouts, and programs/bindings can change after the first observation.

## Intended execution model

1. Run gameplay simulation once. Establish body/head origin and world scale from
   explicit calibration; do not treat the default third-person camera as the eye.
2. Supply a coherent center-head gameplay view early enough for the game's CPU
   scene selection, with a conservative visibility volume covering both eyes and
   any supported late-pose margin. Identify the actual game hooks first. Moving
   the GPU camera after CPU culling cannot restore unrecorded objects.
3. Preserve a submission's immutable camera/projection and resource context. Build
   eye views and asymmetric projections from its reviewed depth convention. The
   existing shaders already convert NVN depth; do not apply that conversion twice.
4. Share genuinely eye-independent work only after reviewing its read/write scope.
   Keep eye-dependent culling results, indirect counts, scene color/depth and
   temporal resources separate. Preserve aliases and initialize any retained data.
5. Replay each eye's required GPU work using private complete camera payloads and
   remapped resources. Do not rerun simulation, increment gameplay counters twice,
   or replay a write into shared original storage. Fitted shadow cascades require
   a deliberate center/union policy, not an accidental second simulation.
6. Retire GPU resources only after completion; reuse allocations after explicit
   fence completion. Track actual image layouts across frames and isolate history
   per eye. A successful tiny resource smoke does not establish these properties
   for the complete game frame.
7. Submit real stereo projection layers with corresponding poses. Then test head
   translation/rotation, near objects, silhouettes, vegetation, shadows, transitions,
   cutscenes and menus, including view edges and room-scale motion.

## Current limits

No gameplay camera override, CPU visibility hook, automatic scene replay or native
stereo projection-layer output is enabled. The standalone eye-resource owner is a
bounded allocation/transfer/lifetime prototype; the full scene inventory is not
routed through it. Physical weapons, body collision and controller interactions
follow the stereo/camera foundation, with the recorded `oot-vr` reference available
for mechanics research. The stable game installation stays separate throughout.

## Observing an engine render boundary

A second possible integration route is to repeat a proven render-only engine phase
from one simulation tick. The per-view uniform writer documented in `nx2/oot_jitter.py`
is an observation point, not evidence that it or any caller is safe to run twice.
Its `main+0x6bc520` address must use the verified main-module base; native linker
addresses and guest addresses are different coordinate systems.

The existing `HOS_SAMPLE` / `HOS_SAMPLE_FILE` profiler was exercised locally against
the main thread in a bounded private-save run, without an XR session or camera
writes. Of 79,895 sampled native stacks, four resolve through the documented view
writer and five through its immediate wrapper. The executable matches the build
receipt; executable, linker map, source fingerprint and profile hashes are associated
in an ignored private report. Raw stacks, generated code and symbol maps stay local.

These observations identify a runtime caller context to audit. Resolution uses
nearest public linker-map symbols, and unwinding, tail calls and inlining may hide
frames. A sample is not a complete call trace or a proof of function boundaries.
No render-only boundary has been established; `safe_to_repeat` remains false.

Before selecting this route, distinguish simulation, view preparation, visibility,
render jobs, NVN recording and presentation in the observed caller chain. Audit
writes and asynchronous tasks, including frame counters, animations, physics,
random state, resource uploads and temporal histories. Associate all observations
with the same simulation/submission identity. Do not invoke the game main/update
loop twice or infer purity from a short static call graph.

Until a repeatable phase is demonstrated, retain the bounded private-resource route
and its coverage gates. The [real uniform upload probe](vr-game-uniform-upload-probe.md)
validates complete private payload transport; it does not choose or enable either
engine replay or renderer replay automatically.
