# Graphics shader effect inventory

`vr/audit_graphics_effects.py` inventories a bounded SPIR-V module against an explicit
Khronos core grammar. It never modifies modules and never marks a draw safe to replay.
It assumes valid SPIR-V; it is not a replacement for SPIR-V validation.

The initial subset accepts a single vertex or fragment entry point, known pure core
operations, read-only image operations and resolved local/output stores. It rejects
storage-buffer/image writes, atomics, unresolved pointer scopes, physical/hidden
loads, pointer conversions/bitcasts, external function bodies, transform-feedback
markers, unsupported operations and unknown extended instruction imports. GLSL math
extensions with pointer operands are rejected too: some have output pointers, so
an extended math import alone is not proof of purity. Copy instructions check both
the source and destination pointer classes. Input modules remain unchanged.

Output/Private/Function stores are per-invocation candidates only. A pipeline may
still capture output through transform feedback, write queries, use unsupported
stages, bind aliased resources or use state not represented by the module. Descriptor
identity, complete resource ranges, camera semantics, visibility, histories and
submission consistency remain separate gates. The report always retains
`safe_to_replay: false`, `safe_to_patch: false` and incomplete symbolic range coverage.

```powershell
python -I -B vr/audit_graphics_effects.py work/PRIVATE-MODULE.spv `
  --grammar PATH/TO/spirv.core.grammar.json --output work/private-effect-report.json
```

Use the [Khronos SPIR-V grammar](https://github.com/KhronosGroup/SPIRV-Headers/tree/main/include/spirv/unified1)
matching the shader toolchain. The local audit uses the already installed
`vulkan-sdk-1.4.341.0` headers. Both module and grammar inputs are bounded to 8 MiB.
Only authored instruction fragments are used in CI; no game or GPU is required.

## Private observations

An offline sweep of modules referenced by camera-related graphics program observations
finds 83 unique modules; 43 fit the initial read-only subset, with no parser errors.
The other modules include nonlocal stores, hidden physical reads/conversions and an
unsupported helper-invocation operation. This is a filter for further inspection,
not 43 replayable programs: observations span private captures, and the binding
older binding-only metadata does not guarantee every stage of a program is
represented when a stage has no uniform bindings. A fresh
[complete program inventory](vr-graphics-program-inventory.md) now covers that gap;
additional draw-state/resource checks remain necessary. No scene replay is enabled from these reports.

Raw modules, hashes, individual reports and capture metadata remain in ignored
`work/validation/graphics-effects-selected-2026-11-13/`. The authored audit source,
tests and this technical summary contain no game shader bytes.
