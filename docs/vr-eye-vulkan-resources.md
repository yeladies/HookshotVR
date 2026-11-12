# Owned per-eye Vulkan resource experiment

`vr/src/eye_vulkan_resources.h` owns two separate single-layer, single-mip,
single-sample 2D color images and two separate uniform/storage-capable buffers.
Each resource has its own dedicated device-local allocation. It is a standalone
prototype: the game renderer does not call it and no XR session is created.

Creation publishes neither eye until all four resources are bound and an
unsignaled completion fence exists. Actual Vulkan memory requirements count
against the caller's total pair budget (hard ceiling 512 MiB). Failures unwind
objects before allocations. Queue submission permits one in-flight operation; the caller supplies a
command buffer and externally synchronized queue belonging to the same device.
The caller owns initialization, image layout transitions, queue-family ownership
and the device lifetime. Resources start undefined, never as valid game history.

The completion fence guards destruction. A timeout or device-loss result leaves
execution uncertain and retains resources through process/device teardown rather
than freeing storage that may still be in use. Callers must keep the device alive
through the owner lifetime. This is not a renderer-wide retirement system.

## Game-free checks

The authored fake-Vulkan test injects all 13 creation/allocation/bind/fence failures,
checks cleanup, budget and memory-type rejection, distinct eye handles and memory,
completion polling, duplicate-submit rejection and uncertain-work retention.
All 67 VR CTest cases pass on Windows.

A separately invoked native smoke is excluded from CTest, so CI needs neither a
GPU nor a Vulkan loader. Build the VR tools normally, then explicitly run:

```powershell
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run
```

It creates no window, display mode change, game process or XR session. It clears
two 32x32 RGBA images red/green, uploads two different complete 1024-byte synthetic eye-camera uniform payloads
into separate buffers, performs explicit transfer/host-read barriers, copies to host-visible
staging, waits up to five seconds for the owner's fence, invalidates noncoherent
memory when necessary, and checks every output pixel and buffer byte. The CPU source is unchanged;
all unknown uniform bytes survive CPU preparation and the GPU upload/readback.
The synthetic calibration/pose values are authored test inputs, not a measured
world-scale or gameplay camera.

Local result on NVIDIA GeForce RTX 2060 SUPER (2026-11-12): 2048 pixels and 2048
buffer bytes correct, fence complete, 10,240 bytes of dedicated resource memory
**total for both eyes**, plus staging. This is a correctness smoke, not a game
performance or full-scene memory measurement. Raw output is private under work/.

## Remaining integration

The observed scene inventory is not yet allocated by this owner. Depth/stencil,
format capability policies, multiple mips/layers/samples, original-image copy
regions and contents, views, descriptor remapping, live address routing, buffer alias
groups, temporal histories and multiple in-flight frames need explicit handling.
Hidden pointers/indirect geometry and compute dependencies remain audit gates.
Do not wire automatic scene replay merely because this small smoke passes.

## Explicit reuse

After a successful completion poll, `rearm()` resets the owned fence and permits
one further submission with the same allocations. Rearming while work is pending,
rearming twice, or submitting twice without rearming is rejected. A failed fence
reset preserves the submission guard. Callers must reset/re-record command buffers
and preserve or deliberately discard actual image contents/layouts themselves.

The updated native smoke makes two submissions with the same resource allocations,
resets the completed command pool, changes each eye's image color and synthetic
uniform tail, and checks all 4096 pixels and 4096 buffer bytes across both rounds.
All checks pass on RTX 2060 SUPER; eye resource allocation remains 10,240 bytes.
The fake test also exercises eight explicit reuse cycles without new allocations.

## Optional addressable buffers

The standalone owner can request buffer device addresses. The caller must enable
`bufferDeviceAddress` on its Vulkan device and supply the address-query function.
The owner adds both shader-device-address buffer usage and the matching memory
allocation flag. Images receive no address allocation flag. Zero, misaligned,
overflowing or overlapping eye address ranges fail creation and unwind allocations.
Default non-addressable creation retains the previous API requirements.

The native smoke now requests Vulkan 1.2, queries support and enables only the needed
buffer-address feature. On RTX 2060 SUPER (2026-11-12), distinct address ranges and
all two-round image/uniform readbacks pass: 4096 pixels, 4096 buffer bytes, four full
synthetic payloads, two submissions, 10,240 bytes of eye allocations plus staging.
The fake test covers missing query support and invalid address ranges.

This validates address-capable storage allocation and transfer completion. It does
not upload a guest root, integrate the CPU page patches with the renderer, or measure
game stereo performance. The following stage adds an authored pointer-access check.

## Authored shader pointer readback

The native smoke now reads each eye buffer through a physical storage-buffer pointer
in an authored compute shader, rather than copying that buffer with a transfer command.
A second physical pointer addresses the bounded output span in staging. Four groups
of 64 invocations copy 256 words per eye per round. Transfer-to-compute and
compute-to-host dependencies precede the CPU comparison and fence-based reuse.
The selected queue must support both graphics and compute.

On RTX 2060 SUPER, 1024 shader-read words (4096 bytes), all four complete synthetic
payloads and 4096 image pixels pass across two submissions. The same allocations are
reused, with source CPU payload unchanged. No game or XR session is created.
This proves direct physical-pointer access for this authored test; guest page-table
indirection and real game shader coverage are still pending.

The checked-in header is generated from `vr/shaders/eye_address_copy.comp`:

```powershell
python -I -B vr/generate_eye_address_shader.py --compiler PATH/TO/glslang.exe
```

Use the [official Khronos compiler](https://github.com/KhronosGroup/glslang/releases).
The local Windows archive used here has SHA256
`e9cbdffc02a708f424aa070202b676aada7af6f5af1f4221ae65e3b331977a46`.
Downloaded tooling remains private under work/. The generator uses Vulkan 1.2,
checks output bounds/magic and records the normalized source hash. A game-free CTest
checks source freshness, instruction boundaries and the compute/addressing contract;
it does not replace SPIR-V validation or the opt-in native GPU run.

## Full-size synthetic guest roots

The next native stage joins the CPU page layout and patch encoder to the authored
shader. Two independent device-local root buffers use the backend's complete
3,145,728-entry topology (24 MiB each). The synthetic source snapshot maps one
4096-byte original buffer at the first page of both guest windows. Each root is
zeroed, populated with those two original entries, then patched to its own private
eye buffer in GPU transfer order. This constructs roots from an authored sparse
snapshot; copying a live game GPU root is not implemented.

The shader dereferences the selected root entry and reads a full 4096-byte eye
payload. Each eye uses the low window in one round and the second window in the
other, so both aliases and both private roots are exercised. The original GPU buffer
is separately read back each round and compared to the authored source. Root writes,
shader reads/writes and host readback have explicit dependencies. Auxiliary storage
shares the submission's observed completion boundary; uncertain execution retains
it together with the device until process exit. The auxiliary allocator permits
only device-local memory and caps actual allocation requirements at 64 MiB.

RTX 2060 SUPER result (2026-11-12): 4096 shader words / 16,384 eye bytes, four complete
synthetic payloads, 4096 image pixels and 8192 original-buffer bytes checked across
two submissions, with all comparisons passing. Eye resources use 16,384 bytes;
the two roots plus original buffer use 50,335,744 bytes, plus staging. Resources
are reused only after the fence completes. No game process, XR session, original
installation writes or display-mode changes occur.

This validates the complete synthetic route from CPU patch planning to physical
shader reads through private guest roots. It does not prove the game's access
inventory complete or route its descriptors, embedded pointers, temporal histories
or camera-dependent compute. Production integration must snapshot the real root at
the correct queue point and preserve all real mappings and resource lifetimes.

## Copy an original GPU root before patching

The current smoke extends the full-root stage with a separate original GPU table.
It is zeroed and initialized with two aliases of a writable candidate page plus a
second, shared-read-only source page. The complete 24 MiB table is copied on the GPU
to each eye root before applying the CPU-generated patches. Explicit transfer
write/read and write/write dependencies separate initialization, copies and patches.
No CPU write substitutes for the GPU copy or alters the mapped original table.

Each eye's private page and unchanged shared page are read through that eye root.
A further shader read uses the original root to verify that its writable-page entry
still reaches original storage. All comparisons use complete 4096-byte authored
payloads, including the unknown-byte tail. The same storage and roots are reused
across two completed submissions, and both guest-window aliases are exercised.

RTX 2060 SUPER result (2026-11-12): all 4096 private-page words, 4096 shared-page words,
2048 original-root words and 4096 image pixels pass. Eye allocations remain 16,384
bytes. Three full roots and the 8192-byte original source use 75,505,664 bytes, plus
staging; auxiliary allocation requirements are bounded by 96 MiB in this stage.
The native output is private in `work/vr-eye-root-copy-native.log`.

This checks GPU-copy preservation and subsequent private patches using authored
inputs. It still does not snapshot the live game's table or prove its shared pages
read-only. In particular, the backend's deferred table uploads must complete before
a real private-root snapshot is copied; reading its CPU mapping while GPU updates
are in flight cannot provide the required immutable snapshot.

A separate [real-game uniform upload probe](vr-game-uniform-upload-probe.md) now
validates complete submitted payload copies on the GPU without binding them to draws.
