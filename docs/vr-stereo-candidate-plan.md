# Conservative stereo resource planning

`vr/analyze_stereo_candidates.py` joins the private compute-binding observations
with the private resource lineage from the same bounded game process. It does not
execute work, allocate resources, alter a uniform or enable stereo replay.

When `vr/run-game.ps1` is run with both `-ComputeShaderUniformAudit` and
`-ImageLineage`, successful diagnostics place the representative graph next to the
unique shader-audit directory's binding metadata. The runner writes a private
association manifest containing SHA-256 digests of both files and the source
fingerprint. The analyzer rejects absent/mismatching associations and missing or
invalid observed modules. This prevents accidentally joining a graph overwritten
by a later run with stale shader observations.

Run with the Builder Python (or another supported Python):

```text
python -I -B vr/analyze_stereo_candidates.py work/game-run/shader-uniform-audit-<id>
```

The tool adds observed canonical/candidate-camera compute programs as conservative
planning seeds, propagates their influence through observed image and storage
buffer versions, and reuses the existing present-slice/inventory/alias analysis.
It reports baseline and expanded per-eye work/resource candidates. The original
capture stays untouched; shared work is never made less private by this analysis.

First-binding observations and a representative frame are not matching evidence
for every dispatch. A program's descriptors may change; unobserved specialized
layouts, hidden pointers, indirect geometry and CPU visibility still require
review. Same-process file association does not prove per-dispatch semantics.
Projection-only coincidences do not seed the plan. `coverage_complete`,
`per_dispatch_camera_semantics_confirmed` and `replay_ready` remain false.

Authored tests cover compute-buffer producer/consumer propagation, unrelated work,
graphics keys, bad versions/IDs, source immutability and association hash failures.
No game data or GPU is required. Full scene Vulkan resource routing and replay
remain separate integration work, as described in the visibility/replay gates.

## Associated live planning result (2026-11-12)

The 150-second combined private-save run completed at 6515 MiB peak working set,
with scene/source-camera and resource-lineage checks observed and no XR session.
The graph/binding association hashes verified. Seventeen compute programs had
canonical or candidate camera bindings; 16 appeared in the representative frame.
Their conservative inclusion adds 10 seed events and 19 dependent events in the
present slice. Per-eye compute-work candidates rise from 17 to 36; graphics stay
at 847. Private buffer range references grow from 69,646,032 to 81,242,064 bytes per
eye. Image allocation references stay at 78,084,768 bytes per eye.

The combined image/buffer reference is 318,653,664 bytes (about 304 MiB) for two
eyes before extra alignment, additional targets, changed resolution and untracked
resources. It is not actual game eye allocation or a performance guarantee.
Coverage, per-dispatch semantic confirmation and replay readiness remain false.
All raw observations, modules, graph and plan remain private under ignored work/.
