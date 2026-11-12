# Real game-image clone probe

`vr/run-game.ps1 -EyeImageProbe` is an opt-in, game-local allocation/copy diagnostic.
It implies the reviewed scene/source-camera capture and does not request an XR
session, native stereo rendering, or a gameplay camera override. The stable game
installation is read-only input; the runner uses its existing private runtime/save
snapshot controls.

After an actually emitted primary-scene draw, the renderer ends its current pass
through the existing `end_pass` path and copies the source color and depth images
into two private images each. Original Texture objects, image handles, guest
registrations and descriptors are not replaced. This is one bounded probe, not
per-eye scene replay or a new image shown in the headset.

## Ownership and memory

`xr_eye_image_factory.h` preserves reviewed native 2D format, flags, extent, mips
and array layers. It supports single-sample ordinary core Vulkan color and
depth/stencil formats; sparse/protected images, 3D textures, multisampling,
multiplane/extension-only formats and unknown creation flags are rejected.
Depth and stencil copies use separate single-aspect regions. Images stay in the
backend's GENERAL layout; private images receive explicit initialization and copy
barriers, followed by dependencies for later work.

A bare-image requirements query happens before either eye allocation. The owner
checks actual allocation sizes, enforces a total pair budget and unwinds failures.
The native probe caps color and depth together at 32 MiB. Its new strict VMA entry
point makes exactly one dedicated, device-local attempt, adding WITHIN_BUDGET when
the device reports that support. It does not use the original allocator's pool,
over-budget or host-memory fallback paths. A missing image fails the probe cleanly.

The probe receives the current draw recorder's actual GPU timeline value, rejects
an already-completed/stale recording value before allocating, and polls
the actual renderer timeline once per later replay frame. Private allocations are
released only after that value completes. A device change or unknown completion
retains resources; it never frees pending images speculatively. The generic owner
also supports extending its last-use value for future users, but requires explicit
retirement before allocator/device teardown.

## Game-free validation

The factory test covers full mip/layer preservation, separate eye allocations,
budget rejection before allocation, partial failure cleanup, depth/stencil aspect
splitting, unknown-completion retention and monotonic last-use retirement. A second
test extracts the actual strict VMA function from `vk_vma.cpp` and runs it with
stubs, checking both driver-budget modes, one-attempt failure and unchanged output
handles on failure. All 64 VR CTest cases pass before the native game probe.

Raw probe JSON is written under a unique ignored work/game-run filename. It reports
copy recording, actual timeline completion and release separately. Completion is
not pixel-by-pixel readback verification, a full-scene resource factory, descriptor
remapping, or proof of native VR. The complete scene inventory and hidden buffer
pointer/indirect dependencies still require separate integration and review.

## Native private-save result (2026-11-12)

The 150-second isolated game run passed the scene/source-camera checks and the
native probe. The sampled source was 1152x648, HDR B10G11R11_UFLOAT (format 122)
and D32_SFLOAT depth (format 126). Four private images (two color/two depth) held
14,155,776 actual allocation bytes in total, 13.5 MiB, within the 32 MiB probe cap.
Copy commands were recorded, the actual recording's GPU timeline completed, and
both pairs were released. Original source objects were not replaced and stereo
replay remained disabled. Peak process working set was 6591 MiB. No XR session,
headset rendering, audio or native camera override was requested.

The final paired native build passed with zero failed compilation units; the final
inherited suite remained 357 passed, five known baseline failures and 59 skipped
(296 seconds, two jobs). All 64 VR CTest cases pass. This confirms allocation,
copy-command completion and retirement, not a pixel comparison or a finished
per-eye scene renderer. Views, descriptor routing and private culling buffers are
still integration work.
