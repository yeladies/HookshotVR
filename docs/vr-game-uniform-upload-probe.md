# Real-game private uniform upload probe

The opt-in `-EyeUniformProbe` stage tests two private GPU copies of a complete
submitted camera binding. It does not replace descriptors, modify source objects,
render either copy or submit an OpenXR layer. Ordinary runs clear its environment
variable; the probe is disabled by default. Renderer ABI remains 18.

The emitted-draw observation must match the pinned primary-camera submission.
The renderer then reads the complete NVN uniform binding through `ub_read`, retaining
submission overlays. Supported payloads are 0x230..65536 bytes, divisible by four.
The source must pass the reviewed rigid-view, projection, view-projection and inverse
checks. Each diagnostic copy changes only the reviewed view/projection fields and
retains every other byte, including unknown mixed data and the unreviewed 0x1c0 field.
The test offsets are +/-0.01 game units; they are unsubmitted synthetic inputs, not
world-scale calibration, a headset pose or permission to use these copies for gameplay.

Two dedicated device-local buffers and dedicated mapped readback storage are bounded
by 1 MiB of actual allocation requirements. Outside rendering, complete payloads are
uploaded, copied back with explicit transfer/host dependencies and compared byte for
byte only after the actual recording timeline value completes. Allocation failures
unwind unsubmitted resources. Unknown completion or device change retains submitted
storage until process teardown. Source bindings and game objects are never replaced.

Shared matrix/copy code lives under `gpu/vk/xr_*.h`, so native header dependencies and
source receipts include it. Existing VR-tool headers forward to the same definitions;
the renderer math has no OpenXR dependency. A regression check confirms each shared
math header invalidates the native source fingerprint while line endings do not.

## Local validation

On RTX 2060 SUPER (2026-11-12), a 150-second run with a private save snapshot and no
window, audio or XR session reaches a matched scene submission. Both complete
13,568-byte payloads match GPU readback; completion and resource release are verified.
Total private/readback allocations are 54,272 bytes, and peak process working set is
6597 MiB. This is transfer/copy validation, not a stereo performance or visual check.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 `
  -Seconds 150 -ExperimentalXrRenderer -EyeUniformProbe `
  -SnapshotStableSaves -LoadTestSave
```

The runner implies scene/camera observations, creates a unique private report and
fails unless full payload comparison, actual completion and release all pass.
Raw logs and reports remain under ignored
`work/validation/native-eye-uniform-probe-2026-11-12/`; no game-derived data is published.

Next gates are complete descriptor/address routing and ownership, unresolved compute
semantics, visibility/culling and temporal resources. Passing this probe does not
establish full scene coverage or enable automatic stereo replay. A headset test is
not needed for this stage.

The 67 game-free VR CTest cases pass. The inherited Windows suite completes with
357 passed, 59 skipped and the same five baseline failures in 296 seconds: the
`fpx_sticky` fixture's missing include, unavailable .NET reference packs in two
Builder fixtures, unavailable symlink privilege in `nup_paths`, and the existing
`vram_budget_row` warning-as-error fixture. This is not an all-green inherited suite.
