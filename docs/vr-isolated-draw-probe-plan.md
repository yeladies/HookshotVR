# Isolated unchanged baseline draw

Status: implemented as a default-off, bounded experiment. The unchanged baseline
passed on an RTX 2060 SUPER on 2026-11-13: one original draw changed 116 pixels and
both private color outputs matched the complete original output exactly. Complete
program inventories alone still do not permit replay; the live draw and current
process stage identities must satisfy the additional guards below. No game update/main loop is repeated,
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

## Read-only native preflight

`vr/run-game.ps1 -BaselineDrawPreflight -SnapshotStableSaves -LoadTestSave`
adds a bounded, default-off native eligibility check. It implies the same-process
shader inventory, image lineage and pinned source-camera capture, with parallel
replay and the recorder disabled. It does not open XR or record extra GPU commands.
The default runner remains headless and muted.

At the ordinary descriptor-build boundary it checks direct triangles, program
stage/write declarations, exact single-sample HDR/D32 targets, all resolved sampled
and storage image reads, owning descriptor snapshots and every primary-camera UBO
alias. It examines existing mirror metadata without importing, rebinding or syncing
memory. Unsupported stream-backed camera descriptors are rejected. A fresh ordinary
descriptor build is required; the check never forces an extra build.

The JSONL report is limited to 256 distinct program/reason pairs. The runner copies
it beside that process's shader inventory and hashes it in capture-association.json.
The offline program audit checks the hash and reports the intersection of native
live-state candidates with reviewed full-stage shader subsets and actual vertex
camera-transform reads. Native eligibility alone does not review shader instructions,
prove physical allocation alias freedom or authorize replay. Every report continues
to declare safe_to_replay=false.

Supporting code now owns complete bounded pushed-descriptor payloads independently
of the renderer writer. A move-only snapshot allows per-eye UBO replacement and a
full explicit source restore. The attempt tracker associates preparation and finish
with the exact draw serial, recording, program, device and command buffer: a skipped
original invalidates preparation before the next draw. Submitted storage can retire
only after known completion on the original device; unknown completion is retained.
The four-frame comparator requires visible original changes and exact pixel equality
for both private outputs, with mismatches taking precedence over invisible output.
The separate isolated-draw mode now uses these components to issue only the two
unchanged private draws described below.


## Bounded GPU recorder and verified result

Run `vr/run-game.ps1 -IsolatedDrawProbe -SnapshotStableSaves -LoadTestSave -Seconds 150`
after building the experimental native renderer. The runner remains headless/muted
unless explicitly changed and refuses XR/session/image-probe combinations. It disables
parallel replay, the recorder and competing query/dump diagnostics for this process.
A fresh capture directory, nonce and native source fingerprint scope the control file.
The Python analyzer writes the selection atomically while the save loads; the native
reader also checks exact program/stage/module fingerprints and word counts from its
own successful module exports. Incomplete/stale selections do not start a draw.

Preparation allocates all private images, views, UBOs and four-frame readback storage
under a 64 MiB actual-allocation budget before recording transfers. It preserves each
camera descriptor's range and copies actual resolved GPU uniform bytes. Pool/capture
stamps must remain unchanged between allocation and preparation. Original target and
read-image VMA allocations must be known; same-image/allocation feedback and sparse
read images reject. The original fragment shader/output mask must write primary color.
No guest resource is imported again or registered for private writeback.

The exact source draw then executes once through its ordinary path. Only its matching
serial/command/recording/program may finish the attempt. Two raw direct draws use the
same pipeline, vertex/index state, push constants and dynamic state in independent
color/depth rendering instances, followed by an explicit full pushed-descriptor restore.
The recorder never re-enters draw() or any game update. Submitted resources remain
owned until the source queue's actual timeline completes; device changes/unknown
completion retain them. Allocation exceptions are contained before recording commands.

At most eight distinct programs are attempted; no visible original pixel change is
inconclusive, while any private color mismatch stops the experiment immediately.
The first test, before the primary-color-write filter, completed eight inconclusive
attempts, with no color differences and all resources released. After adding that
filter, program f0b770574af166d8 passed on the first attempt: 116 changed original
pixels, zero mismatched pixels across both complete 1152x648 private outputs, followed
by verified retirement. Native source fingerprint 0a1eb48b; 150-second process peak
6596 MiB. This proves one unchanged isolated draw, not a full scene, stereo camera,
headset presentation, Linux/AMD behavior or visual gameplay correctness.

Native test coverage compiles the actual recorder against an in-memory Vulkan double:
partial image/buffer allocation failures, allocation exceptions, changed generations,
changed source descriptors, skipped serials, signed indexed offsets, two private draws,
full source descriptor restoration, delayed retirement and deliberate private-output
corruption. Real GPU comparison is a separate local run, never a CI requirement.
