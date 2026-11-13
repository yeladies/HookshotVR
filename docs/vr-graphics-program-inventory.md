# Complete graphics program stage inventory

The native shader audit now emits an explicit stage list independently of uniform
bindings. Every stage in the actually emitted Program is included, including a
fragment stage with zero uniform bindings. Each record retains native/Vulkan stage
identity, its exported module fingerprint, global-memory use and declared writes to
storage buffers, storage images and texel buffers. Duplicate stages and bindings
that refer to an omitted/mismatched stage are rejected. Missing module files remain
explicitly null. Existing binding-only schema-1 captures remain readable but are not
complete stage evidence. ABI 18 and default-off behavior remain unchanged.

Limits remain 64 program records, eight stages per program, 128 uniform bindings,
64 MiB of exported modules and 4 MiB of metadata, including failed write reservations.
The bounded graphics quota is reserved for programs that actually bind the observed
primary camera. Compute-only observation behavior is retained separately.

`vr/audit_graphics_programs.py` verifies every declared stage module, including those
unreachable from the binding list. File contents must match the exporter's FNV filename;
SHA256 identities are included in private reports. FNV is a consistency check, not
cryptographic authentication. Native effect hints and the conservative SPIR-V effect
inventory must both agree. The initial shader subset requires vertex+fragment stages
and primary-camera binding provenance. Observed static byte reads separately identify
whether the vertex stage actually reads canonical view/VP/inverse-view fields; a
binding flag alone does not establish those reads or their complete semantics.

```powershell
python -I -B vr/audit_graphics_programs.py work/PRIVATE-CAPTURE `
  --grammar PATH/TO/spirv.core.grammar.json --require-associated-lineage `
  --output work/private-program-inventory.json
```

The optional required association verifies the same-process lineage/binding manifest
and both file hashes. It does not prove per-draw temporal identity or authenticate
arbitrary external files. Every report keeps pipeline-state/alias coverage incomplete
and `safe_to_replay: false`. A shader subset is not a draw authorization.

## Local result

A fresh 150-second private-save run on RTX 2060 SUPER (2026-11-13), with no window,
audio or XR session, records 64 complete programs and 95 unique stage modules.
All stage files are present and their fingerprints verify. Twenty programs fit the
initial shader-effect subset; nineteen show vertex camera-transform byte reads.
Seventeen of those appear in the associated representative frame. The observed
candidate writes cover color+depth or depth-only images; actual draw arguments,
view ranges, descriptor resolution and state still need live checks.

Peak process working set is 6640 MiB. Raw metadata/modules and reports stay under
ignored `work/validation/full-program-inventory-2026-11-13/` and the unique capture
directory. No game shader bytes or program identifiers are published in this summary.

All 70 VR CTest cases pass. The inherited Windows suite completes in 290 seconds
with 357 passed, 59 skipped and the same five documented baseline failures. Native
compilation passes. This is diagnostic coverage, not a headset or gameplay validation.
