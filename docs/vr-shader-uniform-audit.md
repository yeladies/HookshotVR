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
