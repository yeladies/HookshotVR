# Next experiment: one isolated baseline draw

Status: planned, not enabled. Complete program inventories now identify shader
subsets worth inspecting; they do not by themselves permit replay. The next goal
is to reproduce one unchanged original GPU draw into private color/depth targets,
with both copies matching its original result. No game update/main loop is repeated,
and no modified camera or XR image is presented in this first experiment.

## Required live checks

- Match the current pinned primary-camera submission and exact live program/stages
  to locally inspected module identities. The selected capture belongs to the current
  run/build; no stale list or latest-camera fallback is accepted.
- Begin with direct triangle draws, vertex+fragment only, no mesh/fallback/indirect
  path and no active pipeline statistics query. Require pushed descriptors and strict
  bounds on geometry counts and target dimensions.
- Reject declared global-memory use or shader storage writes and unsupported effects.
  Reject image feedback/aliasing with targets; track all sampled/storage image reads,
  including textures not previously marked GPU-written.
- Begin with one ordinary single-sample 2D HDR color target and D32 depth, mip/layer
  zero and exact native views. Reject sparse/block/reinterpreted targets and unknown
  resource state. Reuse the checked strict image/view factories.
- Copy the real resolved uniform descriptor's GPU data, preserving upstream patches.
  Prove transfer-source usage and logical bounds through a non-mutating native pool
  description. Do not resolve/import/sync a guest buffer again just for the probe.
  Unsupported stream/fallback bindings must reject instead of guessing usage.

## Recording and restoration

Prepare after the ordinary descriptor build and before the ordinary begin_pass and
per-draw bookkeeping. End any earlier pass, snapshot private images/uniform data,
and then let the original path begin its pass and issue the original draw exactly
once. Finish after its ordinary draw counters/bookkeeping. Copy its color reference,
render the same native geometry into each private target, and restore all original
pushed descriptors explicitly. Do not recurse through draw() or rebuild descriptors
mid-frame: resolving memory again can change mirror/upload ordering.

Private rendering uses raw owned views, never shallow Texture copies or registrations
in guest writeback. Preserve the original pipeline, vertex/index state, push constants
and dynamic state. Private pushes must not alter the renderer's cached source binding
records; an explicit full restore must leave actual and cached source state equal.
The existing recording timeline guards every allocation and view.

After completion, compare original pre/post color and both private outputs. Require
visible original pixel changes and byte-identical private copies before accepting the
baseline. A mismatch is a hard failure; no visible output is inconclusive. Any retry
must be bounded and use a different validated candidate. Unknown completion retains
submitted storage. Allocation/setup failures must leave the original path intact.

Only after this baseline passes should camera changes be introduced. Complete source
payload identity, alias routing and matrix semantics still apply. A single-draw probe
must not be presented as a full stereo world or used to bypass compute, culling,
temporal-history and calibration gates.
