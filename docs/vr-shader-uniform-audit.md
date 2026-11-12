# Private SPIR-V uniform-consumption audit

`-ShaderUniformAudit` enables a bounded local copy of modules as they are created,
including modules loaded through the existing program cache. The SPIR-V input and
Vulkan module/pipeline creation are unchanged. Default-off operation writes no files.
The copy is limited to 8 MiB per module, 64 MiB total, 4096 live module identities
and 64 representative emitted camera programs. Data stays under a unique ignored
`work/game-run/shader-uniform-audit-*` directory. These modules are game-derived:
never publish them or turn them into test fixtures.

The observation maps native modules to the actual program's descriptor order, NVN
stage, constant bank, descriptor-array element and range. A primary-camera binding
flag requires that the emitted draw had a successful submitted-camera match and
that the stage's user uniform binding names that checked camera address. The audit
also records other user bindings but does not presume they are camera data. Cache
cleanup fixtures retain their lifetime assertions and additionally check that the
observer is notified only for successfully created modules.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 150 -SnapshotStableSaves -LoadTestSave -ShaderUniformAudit
python -I -B vr/audit_shader_bindings.py work/game-run/shader-uniform-audit-<run-id>
```

The Python reader uses SPIR-V opcodes/decorations verified against the installed
[Khronos unified specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
grammar. It resolves bounded constant accesses through structs, byte-strided arrays
and vectors; descriptor arrays are kept separate from byte offsets. Dynamic indices,
unknown Uniform pointers, unsupported memory copies/strides and out-of-range loads
remain unresolved. It distinguishes storage-buffer or input loads from Uniform
loads. Modules and bindings are never rewritten or executed by this analyzer.
Static footprints are conservative (a vector load may include unused components),
not a complete semantic/dataflow proof. `safe_to_patch` remains false.

## First live capture

The first private-save diagnostic copied 581 distinct modules (38,563,996 bytes),
well within its cap. The representative metadata covered 64 emitted programs and
99 source-checked camera bindings, yielding 82 unique module/binding queries. All
82 had static reads; 80 static subsets resolved and two remained unresolved. No
primary module was missing, and no query failed to parse.

NVN vertex-stage bindings read view/projection regions; fragment-stage bindings
also read the 0xb0 inverse-view candidate. Mesh-stage bindings read view, projection
and VP. Other reads lie beyond the captured 0x230-byte prefix (1142 reported word
entries across the unique queries); their role must be reviewed. A task-stage binding
had no reads in the named prefix regions. These are representative static footprints,
not proof for every shader or camera-dependent compute pass. Do not zero or replace
the rest of a camera buffer, assume all stages share one layout, or infer complete
stereo support from a resolved subset. Actual per-eye resources and draw replay
remain unimplemented.

## Compute-only observations and wider candidate search

`vr/run-game.ps1 -ComputeShaderUniformAudit` enables a separate bounded capture
for emitted native compute dispatches; graphics programs do not consume its
64-program allowance. It implies the existing scene/source-camera capture but
opens no XR session. Replaced upscaler work and skipped dispatches are not treated
as executions of the original compute shader.

The first 150-second private-save run completed at 6641 MiB peak working set.
Metadata covered 31 compute programs (NVN stage 5), 56 user-UBO bindings and no
missing observed modules. None matched the canonical scene c3 block against its
submission context. This is an observation of first emitted bindings, not proof
that compute is camera independent: startup context, specialized layouts, buffer
pointers and later binding changes remain possible.

The subsequent diagnostic adds a 16-byte-aligned search for copied primary view,
projection and composed VP matrices anywhere in each bounded submitted UBO range
(up to 64 KiB). It waits for a nonempty submitted camera context, retains the
250 ms observation/500 ms replay bounds, rejects ambiguous camera identities,
limits each scan to 64 candidate offsets and keeps all bytes read-only. Metadata
contains offsets and eligibility/ambiguity/truncation flags, never matrix values.

A candidate is not a confirmed camera field. Projection alone is particularly weak.
Static read overlap with a view/VP candidate is reported separately from canonical
camera-region semantics. Distinct moving-camera observations and dataflow review
are still required before any specialized block may be rewritten. Missing results
are not proof of independence. All shader/camera patch-safety flags remain false.
Module and metadata write budgets reserve failed/partial writes too; raw records
stay in unique ignored work directories and must never be published.

## Wider compute capture result

The subsequent 150-second private-save run with nonempty submitted-camera gating
completed at 6587 MiB peak working set. It again observed 31 stage-5 programs and
56 UBO bindings, with 44 eligible full-range scans, no ambiguous/truncated scans
and no missing modules. Four bindings matched the canonical camera prefix.
Twenty-one bindings had view or VP candidates elsewhere in their payloads.

Across 21 module/binding queries, all had some static reads; 13 static subsets
resolved and eight retained dynamic/unsupported reads. Static overlap touched view
candidates in six bindings and projection candidates in five; no static overlap
was found with the VP candidates in this representative sample. Canonical stage-5
reads touched the 0xb0 inverse-view candidate region. There were 446 reported read
word entries outside the original 0x230-byte camera prefix across the queries.
No query failed to parse. These aggregate counts do not identify all camera data,
prove the meaning of specialized fields, or authorize live buffer rewriting.

The contrast with the startup-inclusive capture demonstrates why camera context
and observation timing matter: zero canonical matches in an early first-binding
sample did not establish independence. Candidate graph planning must include
these possible compute producers while retaining explicit incomplete coverage.
