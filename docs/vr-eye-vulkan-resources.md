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
All 60 VR CTest cases pass on Windows.

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
regions and contents, views, descriptor remapping, device addresses, buffer alias
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
