# Private-eye guest-page planning

The Vulkan backend translates shader guest pointers through 4 KiB pages. Its current
windows are 8 GiB at `0x1000000000` and 4 GiB at `0x7f00000000`, in that order
(`gpu/vk/vk_memory.cpp`, `guest_pages_set`). The full table contains 3,145,728
64-bit entries: 24 MiB. Two complete private roots would add 48 MiB; this is a
reference estimate, not an allocation performed by the planner.

`vr/src/eye_page_layout.h` computes a diagnostic layout from authored ranges and
explicit mappings. Writes select private spans. Overlapping complete pages and read
descriptor ranges expand those spans to a fixed point, preserving aliases in both
windows. Adjacency alone does not select a page. Each eye receives distinct allocation
indices, while aliases of the same source bytes retain the same offset within an eye.
Alignment prefixes count against the supplied per-eye budget (at most 512 MiB).
The budget excludes image allocations, table roots and Vulkan allocation overhead.

Inputs must carry identical nonzero session, submission, pool-generation and
capture-generation stamps. Unknown mappings, duplicate root entries, incomplete
source pages, out-of-range spans and overlapping native addresses with different
object identities are rejected. Callers must canonicalize those native aliases first.
A failed build preserves the previous output. The stamp is a caller-supplied identity;
this class cannot independently authenticate a capture or detect later pool reuse.
A consumer must revalidate it immediately before using the plan.

Only supplied aliases and read ranges participate. Missing mappings, hidden shader
pointers, indirect inputs, omitted writes and GPU copies remain unresolved.
`replay_ready()` therefore always returns false; shared reads are candidates, never
proof that a resource is independent of the camera. Unplanned shared writes reject.

No live page table, descriptor, buffer or game camera is modified. The backend defers
page-table updates into GPU command order after mirror copies; replacing that with
CPU writes while work is in flight can break ordering. Future integration must preserve
that order, route direct descriptors and embedded pointers consistently, allocate and
retire private resources against actual completion, and validate all accesses.

The game-free `vr_eye_page_layout` test covers cross-page writes, aliases across
windows, descriptor closure, alignment bias, budget limits, stale input identities,
ambiguous native storage and rejection without replacing prior output.

## CPU patch encoding

`eye_page_patches.h` encodes only the changed entries for two future private roots.
It checks the full original 24 MiB snapshot against supplied mappings and requires
matching submission/generation stamps. Destinations use the layout's allocation-index
order, must contain complete page spans, and must be disjoint from each other, all
supplied source objects and every mapped source page (including omitted mappings).
A sorted index of nonzero original addresses avoids rescanning the root per allocation.
Preparation preserves the root and replaces the output only on success. Cross-window
aliases receive identical private addresses within each eye.

These are CPU patches, not Vulkan uploads. They do not allocate roots, copy data,
change descriptors, execute a shader or prove inventory completeness. A future GPU
consumer must first snapshot/copy the original root in queue order, apply patches to
independent roots, bind the matching eye root and retire all referenced storage after
completion. Capture identity is supplied by the caller and must be rechecked at that
integration boundary. Both planners continue to report replay readiness as false.

Upstream review now monitors `vk_memory.cpp`, `vk_bank.cpp` and the SPIR-V emit context
in addition to replay/draw contracts, so pointer-translation changes are included in
candidate update reports and trigger VR CI.
