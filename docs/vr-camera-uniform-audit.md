# Private camera-uniform layout audit

The native stereo path must update all fields a camera-dependent shader consumes,
not just its primary view matrix. `-CameraUniformAudit` is an opt-in read-only capture
of up to eight distinct submitted vertex-camera blocks already accepted by the
source-submission camera matcher. No matrices are written and no OpenXR session is
required. Raw blocks are private game-derived observations; never commit them.

The runner writes under `work/game-run/camera-uniform-blocks.jsonl`. Capturing is armed
by creating the adjacent `.armed` marker after a 3D scene has loaded. Start a short
right-stick camera turn through the private test-input interface before arming, to
sample meaningfully different views. The marker is checked once per replay frame
until armed; at most eight immutable samples are written.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 180 -SnapshotStableSaves -LoadTestSave -ImageLineage -CameraUniformAudit
# In another terminal after confirming the 3D scene:
Add-Content work/game-run/input-live.txt '0:RRIGHT/1200,3:RLEFT/1200'
New-Item work/game-run/camera-uniform-blocks.jsonl.armed -ItemType File
python -I -B vr/audit_camera_uniforms.py work/game-run/camera-uniform-blocks.jsonl
```

The analyzer validates the known row-major view at 0, VP at 0x30 and projection at
0x70, then searches aligned blocks for algebraic inverse-view, inverse-VP and
inverse-projection candidates. Candidate offsets must agree across all supplied
samples. Repeated views do not count as independent confirmation. Algebraic equality
does not prove shader consumption or safe patchability; `safe_to_patch` stays false.
World scale, player/body anchoring and all temporal-camera state remain unresolved.

## Checked-at-submit host refresh

The existing matrix-construction hook and projection observer need not publish in a
matching order or rebuild every cached field every frame. The scene diagnostic now
also reads the current primary-camera chain at submit boundaries. It checks readable
memory, a bounded nonempty camera list and the known view vtable, and requires two
matching reads before publishing. On Windows the copy uses ReadProcessMemory on the
current process; the Linux path uses bounded committed/readable-region checks.

A bounded history of sixteen projection-object addresses comes only from the existing
build-guarded observer. At refresh they are read afresh and compared to the validated
primary view. Auxiliary views cannot replace main-view provenance solely by being
published last. Debug/unavailable cameras invalidate the cache. Refresh happens at
least once per presented frame, with a 30 ms fallback interval when the present
counter is unchanged. The existing receiver still enforces finite/rigid matrices,
presentation association, age and final submitted-UBO matching. No age limit is
extended. Adjacent copies are observational, not a transaction across all guest
writers, so they still do not authorize a camera override.

## Live validation of checked camera refresh

The 180-second flat private-save test with lineage and manually armed uniform audit
passed its source-submission and lineage gates. The last camera aggregate reported
10,397 primary observations, 25,002 uniform observations and 24,996 matrix matches;
one packet was rejected. The last draw
aggregate accepted 2,018,447 of 2,460,000 candidates, with zero missing source contexts
and 5,470 ambiguous camera identities rejected. These remain observations, not unique
simulation ticks or completed GPU draws. Peak sampled working set was 6671 MiB.

Eight samples from consecutive replay frames during a scripted camera turn gave eight
distinct views. Strict algebraic comparison found the view at 0, VP at 0x30, projection
at 0x70 and inverse view as a 3x4 block at 0xb0. Projection-like data also matched at
0x1c0; its semantic role is unresolved. No 4x4 inverse-view, inverse-VP or inverse-
projection block was established. Shader consumption is still unverified and the
audit explicitly reports `safe_to_patch: false`. No camera uniform was overwritten.
The raw blocks, frame graph and logs stay private under ignored work directories.
