# Native stereo development checkpoint

The live Quest test has confirmed flat game-image transport on a world-anchored quad.
The current game still has no native stereo output or head-driven gameplay camera.
No new headset test is required for the completed diagnostics below.

## Implemented and checked

- Paired isolated host/renderer builds, ABI 18, reviewed game ID and source/binary receipts.
- Source-submission camera snapshots with guarded live host refresh and expiry/identity rejection.
- Conservative emitted-command image/storage-range lineage; present-slice analysis and alias closure.
- Image inventory/reference budgets and a pure per-eye buffer address layout.
- Complete [program stage inventories](vr-graphics-program-inventory.md): 64 real programs, no missing modules; 19 shader-subset candidates have observed vertex camera-transform reads.
- Conservative [graphics effect inventory](vr-graphics-effect-audit.md), rejecting shared writes and unresolved effects; full program-stage coverage remains pending.
- Validated [buffer descriptor candidates](vr-buffer-descriptor-routing.md); authored GPU checks UBO/SSBO/root agreement and detects deliberately crossed eye bindings.
- Diagnostic [guest-page closure planner](vr-eye-guest-pages.md), with generation checks and alias/read-range expansion, plus atomic CPU address-patch encoding.
- Standalone bounded Vulkan eye resource owner; native 32x32 image/buffer readback, disjoint buffer device addresses, authored shader reads through copied/patched full-size private guest roots with shared/original preservation and explicit reuse pass on RTX 2060 SUPER.
- Strict VMA image/view factory; a real 1152x648 game HDR/depth probe allocates/copies/retires four images (13.5 MiB) and four views. Full scene routing is still absent.
- Private eight-view uniform audit and representative SPIR-V read-footprint audit.
- Separate compute-only observations, plus bounded submitted-matrix candidate search (not semantic proof).
- Reviewed NVN projection-depth extraction, with unresolved/infinite far-plane handling.
- Validated full-payload CPU eye uniform copies, retaining every unknown byte.
- Opt-in [real-game uniform upload/readback](vr-game-uniform-upload-probe.md): both 13,568-byte private payloads validate and retire; source descriptors remain unchanged.
- 70 game-free VR CTest cases pass; inherited suite has five documented baseline failures.
- Read-only native profiling identifies a view-writer caller context; a repeatable render-only engine phase remains unproved.

Raw graphs, matrices, modules, generated game code, binaries and save snapshots stay
under ignored work directories. Only original source, tests and documentation belong
in the branch. The stable installation remains read-only input.

Next bounded experiment: [one isolated unchanged draw](vr-isolated-draw-probe-plan.md).

## Next integration gates

1. Integrate the validated full-payload CPU eye copies with reviewed descriptor routing.
   Complete specialized compute-field semantics first. The extra projection-like data at
   0x1c0 and other fields still need semantic review; unknown fields must be retained.
2. Validate CPU scene visibility and a conservative center/two-eye volume, plus
   camera-dependent compute/culling, hidden pointers, indirect geometry,
   copies/aliases and temporal resources not fully covered by the current lineage.
3. Extend the checked image/buffer owners to the complete scene inventory and remaining view formats,
   initial copies and descriptor/address
   remapping and fence-based retirement. Never repeat writes into shared original storage.
4. Replay the necessary camera-dependent work into independent eyes from one simulation
   tick. Match the NVN -1..1 depth path: the existing shader already converts to Vulkan.
5. Submit actual stereo projection layers with coherent poses and explicit world-scale,
   body/head calibration. Request the user's visor only for a new visual check at that stage.

The shader audit resolves a static subset, not complete semantic proof or permission
to rewrite every buffer. World scale, first-person body integration, room-scale
collision, controller weapons and comfort remain development work. The classic
`oot-vr` reference remains recorded for the later mechanics stage.

See [visibility and replay gates](vr-culling-and-replay.md) before enabling a game camera override.

## 2026-11-13: isolated-draw preflight checkpoint

Native source fingerprint `9fa74f11`, ABI18, upstream base unchanged. The 150-second
headless/muted private-save run verified primary camera submission, lineage and shader
capture in one process. Its 149 bounded native preflight records produced 18 programs
that also pass full-stage shader-subset and actual vertex-transform-read review.
Source mirror transfer usage and camera UBO aliases are now checked without another
buffer resolution. Seven target-feedback programs and one unresolved-image-read case
were rejected. These counts are distinct program/reason pairs, not draw counts.

Validation: native build 74 seconds, zero failed translation units; all 74 VR tests
pass (22 program-inventory cases after the Windows SHA-256 case fix). The inherited
suite reports 357 passed, 5 known environment/baseline failures and 59 skipped in
298 seconds. Failures remain fpx_sticky, builder_details_hitbox, builder_dlss_update,
nup_paths and vram_budget_row. A separate 20-second default-off headless startup
completed its time limit with a 3000 MiB peak; no gameplay validation. All 19 stable
installation hashes still match. Linux/AMD and headset visuals remain untested here.

Private evidence: work/game-build/baseline-preflight-live-result.txt,
work/validation/baseline-preflight-programs-2026-11-13.json,
work/game-build/inherited-baseline-preflight-tests.log,
work/vr-baseline-preflight-final-tests.log and baseline-preflight-default-off-result.txt.
Raw reports/captures/binaries remain ignored and are not publication inputs.

Next: implement the baseline-only GPU recorder described in vr-isolated-draw-probe-plan.md.
Use the current-process nonce/source selection protocol and exact exported stage
identities, then the live eligibility predicate. Copy source GPU uniform bytes and
pre-draw color/depth into owned resources before the original pass. Finish only the
same draw serial, issue the exact direct native geometry twice, and explicitly restore
the owning full descriptor snapshot. Compare original before/after and both private
outputs only after known queue completion. XrBaselineAttempt and the four-frame
comparator are implemented/tested, but no extra game draw is implemented or enabled
by this checkpoint. No headset test is needed for this next baseline experiment.

## 2026-11-13: unchanged isolated GPU draw passed

The default-off `-IsolatedDrawProbe` is now implemented. Current native fingerprint
`0a1eb48b`, ABI18, pinned upstream base unchanged. A fresh same-process nonce/source
selection is produced atomically during the bounded run; live native stage/module
identities must match before any private work. The ordinary source draw executes
once, two raw unchanged private draws follow, and the full original pushed set is
explicitly restored. No game update/main loop is repeated and XR is not opened.

Actual RTX 2060 SUPER result: program f0b770574af166d8, 1152x648 HDR/D32, two camera
UBO aliases with a maximum resolved range of 2432 bytes, thirteen read images. The
original draw changed 116 pixels; both complete private color outputs matched bitwise.
The first eligible color-writing attempt passed and private resources were released
after the native timeline completed. Peak process working set 6596 MiB in the
150-second headless/muted private-save run. The earlier unfiltered run exhausted eight
invisible attempts; those were inconclusive, never accepted as a match. Color-write
and known VMA allocation/feedback checks now exclude those cases.

Validation: 75/75 VR tests pass, including a fixture compiling the actual recorder
against a Vulkan double with allocation exceptions/failures, stale generations,
changed descriptors, skipped serials, signed indexed geometry, full source restore,
delayed retirement and deliberate private-pixel corruption. The inherited suite
reports 357 passed, the same 5 known baseline/environment failures and 59 skipped in
295 seconds. A 20-second default-off startup completed with both new flags false,
3001 MiB peak. All 19 stable installation hashes still match. Linux/AMD and headset
visuals have not been tested by this experiment.

Private evidence: work/validation/isolated-baseline-2026-11-13/{result.json,
baseline.json,attempts.jsonl,stderr.txt}; work/game-build/baseline-draw-color-live-result.txt;
work/game-build/baseline-draw-default-off-result.txt;
work/game-build/inherited-isolated-baseline-tests.log;
work/vr-baseline-draw-final-tests.log. No captures, binaries, game shaders or assets
are publication inputs.

Next work: review the same draw's complete camera aliases/read footprints before
introducing eye transforms, then expand coverage to complete producer/consumer
passes. The current baseline copies exact resolved GPU UBO ranges, not necessarily
the whole logical NVN camera allocation. Do not assume every camera alias starts at
its base address, or that a single successful draw establishes full scene replay,
culling, temporal histories or world scale. Keep synthetic camera changes isolated
and prove routing/completion before any headset presentation. A visor test is not
needed yet; no new stereo scene is presented by this milestone.

## 2026-11-13: frozen 3D game-scene headset build prepared

A private build is ready at work/headset-preview/Probar-en-Quest.cmd. It is a depth-
reconstructed FROZEN game scene with true per-eye projection and room-space head
movement, not live native game stereo. It is intended to check world scale, orientation
and parallax before extending the native eye renderer. No game updates/physics/input
run in the viewer, and hidden/out-of-capture surfaces are missing. User readiness for
the Quest/Virtual Desktop test was requested; do not launch an XR session until the
user says the headset is connected and can be worn.

Capture source fingerprint 38d6a5db, ABI18: same-frame matched primary HDR/D32 inputs
at the known upscale boundary; actual sampled RGB9E5 view format123 recorded in V2.
The first B10 interpretation was visually wrong and corrected after confirming the
actual view format. Viewer GPU smoke without XR: 583489 non-black pixels, 257604
stereo-different pixels. Both images inspected locally; 77/77 VR tests pass. Inherited
suite 357 passed, the same 5 known failures, 59 skipped in294 seconds. Default-off
startup checked; all19 stable installation hashes unchanged. Active runtime registry
points to Virtual Desktop's virtualdesktop-openxr.json. New headset perception still
pending; do not claim playable/native full-scene VR.

See docs/vr-frozen-depth-preview.md for exact scope and reproduction. Private evidence:
work/game-build/depth-scene-format-capture-result.txt; work/validation/depth-mesh-build.json;
work/validation/depth-scene-offscreen.json; work/vr-depth-scene-tests.log;
work/game-build/inherited-depth-preview-tests.log. Captures, mesh, PPM/PNG, binaries
and launch package stay ignored/private and must not be published.

## 2026-11-13: moving depth-reconstructed scene verified without XR

User is remote and explicitly deferred headset testing. No XR session was opened.
The frozen package remains available; a separate moving package is prepared at
work/headset-live-preview/Probar-en-movimiento.cmd. It runs the isolated game producer
and a GPU depth-reconstruction viewer together, then closes both via a scoped stop
signal. Preparation takes about90 seconds; viewer duration is60 seconds. This is a
LIVE DEPTH PREVIEW, not native full-scene VR or integrated VR controls/audio.

Native capture fingerprint d0fbac6a, ABI18. Atomic latest-frame replacement and
same-frame camera/color/depth matching preserve complete V2 packets. Cached VMA
readback is reused only after known timeline completion, capped at16MiB. Native
interval is at least100ms (default200ms), maximum300 frames. The viewer keeps one
16MiB mapped storage buffer, updates it between completed stereo pairs, and rebuilds
the grid in an authored vertex shader. Stale streams stop submitting a scene layer
after2 seconds. Both eyes use the same accepted frame and runtime poses/FOVs.

Optimized real-game test,15 seconds, no XR:67 accepted/rendered pairs (~4.5 updates/s),
14937386 temporal pixel changes, mean render/complete/copy1458us per800x600 pair and
verification readback/file7550us perpair. Producer70 frames, exactly1 readback
allocation (5971968 logical bytes), maxpublication12101us, game peak6612MiB. Cleanup
returned stop_requested and capture-completed true. Private scope:
work/live-preview-056b8fd34af849e2b06d46b3da26d952/.

A synthetic GPU test kept color constant and changed only depth:45 frames,0 source
color-change frames,44 depth-change frames,1659573 changed output pixels. It verifies
geometry updates and host/GPU visibility independently of texture animation. The
explicit tool vr/tests/run_depth_motion_gpu.py reads no game and never opens XR.

All78 VR tests pass. Inherited regression:357 passed, the same5 known baseline/
environment failures,59 skipped in293 seconds. Default-off20-second startup completed
with both depth flags false, peak3003MiB. All19 stable installation hashes unchanged.
Evidence: work/validation/live-depth-final-orchestrated-test.txt,
work/validation/depth-only-motion-gpu.json, work/vr-live-depth-release-tests.log,
work/game-build/inherited-live-depth-tests.log and docs/vr-live-depth-preview.md.

Next: user headset checks for scale/orientation/parallax/performance when physically
available. Main native eye-renderer/camera-alias work remains outstanding; depth
reconstruction does not solve hidden surfaces, native culling, temporal state or VR
interaction. Never publish work/, captures, packets, meshes, PPM/PNG or binaries.
