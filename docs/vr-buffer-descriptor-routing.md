# Private buffer descriptor candidates

`vr/src/eye_buffer_descriptors.h` turns a reviewed buffer layout into separate copied
`VkDescriptorBufferInfo` lists for each eye. It supports explicit finite uniform and
storage buffer ranges. It does not push game descriptors, allocate resources, or
authorize replay; `replay_ready()` remains false.

A matching nonzero session/submission/pool/capture stamp is required. Source objects
must be canonicalized, and private handles must be distinct from every source and
from each other. Each binding is checked against source/destination sizes, actual
physical-device alignment/range limits and declared buffer usage. Partial private
coverage, unplanned writes, uniform writes, dynamic descriptors and `VK_WHOLE_SIZE`
are rejected. Shared reads require explicit caller review. Failure leaves the prior
output unchanged, including failure after earlier bindings were prepared.

These checks consume caller-supplied metadata. Distinct handles do not prove that
underlying memory allocations do not alias; the owner must establish that separately.
A stamp does not authenticate a live resource or prove inventory completeness. The
caller must revalidate identity/lifetime at recording and include all embedded pointer
and descriptor aliases in the same closure before enabling real scene replay.

## Native agreement test

The authored address-copy shader now reads every private word through three paths:
the copied/patched guest root, a storage descriptor, and a std140 uniform descriptor.
The descriptor planner routes both binding types to the same eye allocation. The
shader marks any disagreement by complementing that word before host readback, so
comparison against the complete authored payload verifies all three paths together.
Each eye has its own descriptor set; no live game's state cache is touched.

The native smoke also retains its shared-page and original-root checks, image reads,
completion fence and two-round reuse. Two explicit negative modes swap only one
binding type between eyes, while requiring images, shared pages and the original
root to remain correct. Both eyes must report a payload mismatch in each round.

```powershell
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --expect-ubo-rejection
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --expect-ssbo-rejection
```

All three modes pass on RTX 2060 SUPER (2026-11-13): the ordinary run verifies agreement;
the negative runs detect the expected misrouting. Each checks 4096 private words,
4096 shared words, 2048 original-root words and 4096 image pixels over two submissions.
Eye allocations remain 16,384 bytes and auxiliary buffer allocations 75,505,664 bytes,
plus staging and descriptor/pipeline driver storage. These are correctness tests,
not a game performance or complete VRAM-budget measurement. No game, window, display
mode change or XR session is involved; native execution remains opt-in and outside CI.

The game-free descriptor test covers private reads/writes, explicit shared-read review,
atomic failure, partial coverage, bad handles/usage, unsupported dynamic offsets,
alignment/range bounds, overflow and stale identities. The VR suite contains 69 tests.

Next integration must cover image views/attachments, actual game descriptor bindings,
compute writes and hidden pointers under a single resource/submit identity. This
prototype does not establish that the game's scene is safe to replay.
