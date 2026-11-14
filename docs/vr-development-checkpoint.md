# Native stereo development checkpoint

The playable Quest experiment has head-tracked game-image transport and audio, but its temporal eye pairing still flickers and exposes image boundaries. It does not yet produce a complete native same-state stereo scene. No headset test is requested while the user is unavailable. The dated sections below preserve earlier development checkpoints.

## Implemented and checked

- Paired isolated host/renderer builds, currently ABI 20, reviewed game ID and source/binary receipts.
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
- 106 game-free VR CTest cases pass; inherited suite has five documented baseline failures.
- Read-only native profiling identifies a view-writer caller context; a repeatable render-only engine phase remains unproved.

Raw graphs, matrices, modules, generated game code, binaries and save snapshots stay
under ignored work directories. Only original source, tests and documentation belong
in the branch. The stable installation remains read-only input.

The [isolated unchanged-draw plan](vr-isolated-draw-probe-plan.md) is an earlier completed diagnostic; current work is tracked in the dated sections below.

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

## 2026-11-13: headset feedback and native camera alias closure

The user verified that the moving reconstructed scene stays visible and moves in
Quest 3, with apparently correct scale/depth but pixelated appearance. The session
accepted 254 captures in 60 seconds and closed both processes normally. A previous
launch opened after its producer exited and briefly showed a stale frame; the source
launcher now rejects dead/stale producers before XR and removes intermediate console
writes. The root cause of that delay is not proven.

Native work now closes overlapping resolved camera UBO ranges transitively, preserves
per-descriptor relative offsets/ranges in each eye copy, and rejects storage aliases,
overflow and unions above 64 KiB. It does not identify all logical camera bytes,
physical aliases across different buffers or shader matrix semantics. No camera
transform or full-scene native stereo is enabled. See vr-native-camera-aliases.md.

Native source 7836e412 compiled successfully. Actual GPU baseline matched on its
first attempt: 117 changed original pixels, no differing pixels in either private
output, resources released; run peak 6548 MiB. The run remained headless/muted and
opened no XR session. All 78 VR tests passed; additional offset-chain/overflow cases
passed in the actual-recorder test. Default-off startup passed and 19 stable file
hashes remained unchanged. Inherited regressions completed: 357 passed, the same five known baseline/
environment failures and 59 skipped in 374 seconds (two workers).

Private evidence: work/game-build/native-alias-live-result.txt,
work/game-build/native-alias-default-off-result.txt,
work/game-build/inherited-native-alias-tests.log,
work/vr-native-alias-tests.log and work/validation/native-alias-stable-hashes.json.
No captures, game shaders, binaries or game files are publication inputs.

## 2026-11-13: live game head-camera headset test prepared

A separate private launcher is ready at
work/headset-immersive-preview/Probar-juego-inmersivo.cmd. It uses the running game,
a guarded main-camera override and a monoscopic OpenXR projection layer. It is not
a depth reconstruction and is not native stereo: both eyes receive one source
camera image/pose/FOV. Third-person anchoring, limited FOV, culling/transition issues,
head collision, VR controller actions, audio routing and headset comfort remain
unverified or unfinished. No new XR session was opened during this development run.
The first headset perception test is pending user readiness.

The host/renderer interface intentionally advances to ABI19 for head_camera. Old
binaries/receipts must not be mixed. Guarded gameplay camera writes retain the known
vtable/camera-zero checks and exclude debug free camera. The pose helper preserves
pitch/yaw/roll and translation, bounds pose age and motion, handles origin changes
and avoids feedback accumulation. Each image requires a matched submitted camera
and exact present tag; missing/stale/changed-origin metadata suppresses projection.
No game update is repeated per eye. This does not establish full stereo replay.

Final native fingerprint 8e6576af builds. A 120-second synthetic-pose test ran the
real game without XR: immersive_camera_verified=true, 725 camera overrides, 491208
matched draw observations, 719 tagged presents and 360 associated published images,
6550 MiB peak. These counts are not headset FPS or latency. The earlier 150-second
implementation run recorded 1206 overrides/600 associated images at 6677 MiB peak.

All82 VR tests pass, including actual projection-layer submission against a fake
runtime, rejection of images without matching metadata, native camera math and
launcher guards preventing synthetic/XR combinations. Final inherited suite:
357 passed, the same five baseline/environment failures,59 skipped in316 seconds.
Default-off startup passed; all19 monitored stable hashes remain unchanged.
Source wrapper syntax and its Windows named mutex were checked. The package has
launcher/docs/receipt only and uses the privately built matching host/renderer.

Evidence stays private: work/game-build/immersive-synthetic-final-result.txt,
work/validation/immersive-synthetic-final-report.json,
work/vr-immersive-final-tests.log,
work/game-build/inherited-immersive-final-tests.log,
work/game-build/immersive-default-off-result.txt,
work/validation/immersive-stable-hashes.json. See vr-immersive-game-preview.md for
English/Spanish instructions. Game binaries, images, saves and game files are never
publication inputs. A new Hookshot upstream version has not been integrated.

## 2026-11-13: wide game lens and temporal eye stereo prepared

The user confirmed smooth mono gameplay/head response and original controls, but
reported a narrow head-following window, silent audio and black menus. That test
submitted888 new game images and closed without XR API errors. Its audio was off.
The requested next priority is source FOV and distinct eye views.

The new ABI20 path supplies present identity and a tracked lens. A fresh lift of
four reviewed perspective construction sites changes matrix inputs and stored
CPU-frustum fields, default-off. The engine field is tan(vfov/2), not radians;
host_vr_lens_fields validates and converts it. Each eye uses its runtime pose once,
anchored at the head centre. Eye capture phases cover two ordinary presents so the
existing every-second-present readback can sample both; no game simulation or GPU
compute replay is repeated per eye. This is TEMPORAL stereo, not same-frame native
stereo replay. Pair age, origin, duplicate/order/present and eye identities are
checked. GPU upload uses two disjoint1280x720 regions of a2560x720 swapchain.

Final source c5617507, ABI20: actual game/offscreen check stereo_ready=true and
distinct_pair_verified=true. Requested/observed vertical FOV1.74533rad (100deg),
horizontal1.91986rad (110deg), both eye images accepted, peak6632MiB. Target angles
were synthetic; Quest targets come from its runtime eye FOVs. The menu check failed:
no menu image accepted while open; closing it restored the game. Do not claim menus
fixed. Audio is requested by the new private launchers but not heard/verified yet.
No new XR session was opened during implementation; headset perception is pending.

All88 VR tests pass, including generated hook/culling-field compilation, engine
unit conversion, eye offsets, pair policy, different image/subrect uploads, missing
eye suppression and menu transitions against a fake runtime. Final inherited suite:
357 passed, same5 baseline/environment failures,59 skipped in317seconds. A20-second
default-off startup passed at3005MiB; all19 monitored stable hashes match.

Private launchers: work/headset-stereo-preview/Probar-solo-FOV.cmd and
Probar-fov-estereo.cmd. Source launcher: vr/run-immersive-preview.ps1 with WideFov
or TemporalStereo and optional Audio.180seconds including about90s preparation.
The first headset check should use a still scene and small movements. Third-person,
head collision, physical VR controls, temporal ghosting, menu coverage and sharpness
remain limited/pending. Source instructions are bilingual in
vr-temporal-stereo-preview.md. No game assets, generated game code, binaries, saves
or captures are publication inputs.

Evidence: work/validation/stereo-game-corrected-result.json;
work/stereo-check-f4d1dbe0421547499bd85ba7e8f4dd4d/;
work/vr-stereo-corrected-tests.log;
work/game-build/inherited-stereo-corrected-tests.log;
work/game-build/stereo-default-off-result.txt;
work/validation/stereo-stable-hashes.json. The existing base is unchanged; this does
not integrate the newer upstream development branch or promise future compatibility.

## 2026-11-13 — Remote source-eye inspection and menu diagnosis

ABI20 source `04e3fcb8` builds on Windows. All 91 VR tests pass, including bounded private PPM capture, distinct eye input, menu portrait classification and rejection of a resized main-aspect scene without camera provenance. The inherited suite remains 357 passed / 5 known baseline-environment failures / 59 skipped (336 seconds). Default-off startup ran for its 20-second limit at 3023 MiB peak. All 19 monitored stable-install hashes remain unchanged.

Two actual-game, synthetic-pose, non-XR checks produced distinct temporal eye views at 110° horizontal and 100° vertical. The second recovered from the menu with 72 left and 70 right source images. Private captures show the game world and inventory; they are excluded from Git. Continuous inventory delivery still fails. Diagnostics identify a 1536×864 perspective candidate while the tracked world target is 1152×648; both have the same aspect. Resolution or aspect alone cannot safely distinguish a UI portrait from dynamic scene resolution. Retain rejection of unassociated 3D frames until a source-backed inventory/render-target identity is established. One or two accepted menu images do not establish a continuously visible menu.

Wide-view screenshots also show edge geometry needing culling investigation. Next human gate is wide-FOV / temporal-stereo perception and audio in Quest; no headset session was opened remotely. Same-simulation-frame stereo, first-person body handling and motion-controlled weapons remain separate work.

Castellano: build ABI20 `04e3fcb8`, 91 pruebas VR correctas, regresiones 357 correctas / 5 fallos conocidos / 59 omitidas. Arranque sin VR verificado y 19 hashes estables intactos. Las capturas privadas reales muestran ambos ojos y el inventario, pero la entrega continua del menú sigue fallando: un candidato 1536×864 comparte aspecto con la escena 1152×648. Hace falta identificar su origen, sin usar únicamente resolución ni relajar el rechazo de escenas sin cámara. Revisar también el culling en los bordes. La próxima comprobación humana debe validar FOV, estéreo temporal y audio; no se ha abierto XR en remoto. No se implementaron todavía estéreo simultáneo, cuerpo en primera persona ni armas físicas.

## 2026-11-13 — Same-state eye patch footprint gate

Added an offline per-program camera patch footprint report; see [same-state eye patch preflight](vr-same-state-eye-patch.md). An associated private inventory contains 64 programs, 16 read-only baseline candidates with transform reads: 12 read only within the current private matrix patch extent, 4 also consume preserved fields requiring review. Missing/unresolved reads remain blockers and all reports keep safe_to_patch=false. Baseline GPU behavior and selection are unchanged. This advances the same-state stereo investigation without claiming a transformed GPU draw or full scene replay. Program-audit fixtures: 28 passing tests. Detailed shader reports stay private.

Castellano: nueva auditoría por programa de las lecturas que cubriría el parche de matrices. Captura asociada de 64 programas; 16 candidatos existentes, 12 dentro del prefijo contemplado y 4 con campos conservados pendientes de revisar. Se mantienen los rechazos de lecturas desconocidas y safe_to_patch=false. No cambia el baseline ni se afirma haber transformado un dibujo GPU o la escena completa. Pasan 28 pruebas del analizador. No hace falta el visor para este paso.

## 2026-11-13 — GPU eye-matrix transport verified in the actual game

ABI20 native source `e0b6e4f9`. The authored GPU shader produces two private camera payloads from the same GPU source and recomputes coherent view-projection/inverse-view matrices. Positive GPU smoke, excessive-shift rejection, invalid-view rejection and the previous GPU smoke all pass on RTX 2060 SUPER. New recorder fixtures cover all 12 setup failure points, delayed retirement, original descriptor restoration and deliberate corruption of preserved fields/source bytes. All 91 VR tests pass. Inherited suite: 357 passed, the same 5 baseline/environment failures, 59 skipped in 342 seconds.

The isolated real-game transport run passed on its first attempt: a complete 2432-byte source range copied and patched for both eyes, exact source-before/source-after equality, both private payloads validated, 112 visibly changed original pixels, zero image mismatches in either unchanged baseline draw and all resources released after completion. Peak process memory was 6628 MiB. `patched_buffers_bound_to_draw=false`: no game draw used the transformed buffers. No game update was repeated and no XR session opened. Full-scene same-state stereo and transformed-draw image validation remain pending.

Castellano: fuente ABI20 `e0b6e4f9`; shader GPU propio, pruebas positivas/negativas y 91 pruebas VR correctas. Regresiones: 357 correctas, los 5 fallos conocidos y 59 omitidas. El juego real validó al primer intento dos copias transformadas del mismo uniforme de 2432 bytes, fuente intacta, 112 píxeles de dibujo original, cero diferencias en ambos dibujos de referencia y recursos liberados. Pico de proceso: 6628 MiB. Las copias transformadas todavía no se enlazaron al dibujo. No se repitió la simulación ni se abrió XR. Falta probar el dibujo transformado y después los pases completos de la escena.

## 2026-11-13 — Same-state real depth draw with opposite lens shifts

See [isolated same-state lens draw](vr-same-state-isolated-draw.md). ABI20 source `ae18b93d` passed its first actual-game D32 attempt: 811 changed original pixels, two exact unchanged controls, shifted masks of 811 pixels each at horizontal centroids 575.29 / 571.29 / 579.29 and identical vertical centroid 517.589. Both complete 13568-byte private camera payloads validated; the source stayed exact; resources retired. Peak process memory 6471 MiB. No game simulation repeat or XR session. This is one depth draw and a diagnostic lens shift, not complete shaded stereo or physical parallax.

All 92 VR tests pass, including protocol separation, native argument forwarding, depth/color controls, all 16 setup failure points, rejected object extents and raster-mask failure cases. The PowerShell selector regression initially split a scalar flag into characters; typed array forwarding now has a game-free test. Shader-used ranges remain intact while a proven complete NVN object extent is copied. The candidate/live-state audit showed the simple matrix-only shaders were depth passes; color preflight requirements remain unchanged outside the explicit lens experiment.

Castellano: primer dibujo real de profundidad desde el mismo estado con dos referencias exactas y dos lentes desplazadas ±4 píxeles. 811 píxeles en las tres máscaras, uniforme completo de 13568 bytes validado, fuente intacta y recursos liberados. Pasan 92 pruebas VR. No se repitió la simulación ni se abrió el visor. Sigue pendiente la escena completa y el paralaje de cámaras trasladadas; el modo jugable continúa siendo temporal.

Inherited regression for the final D32 implementation: 357 passed / 5 known failures / 59 skipped in 298 seconds. Default-off startup passed its bounded 20-second run at 3063 MiB, XR disabled.

## 2026-11-13 — Quest feedback, preview margin/quality and pair diagnostics

The user tested both modes: temporal stereo flickered black, mono stayed visible but looked pixelated, and both retained rectangular black boundaries. Source reports observed ~108° horizontal / 110° vertical; the old transport was 1280×720. Added opt-in 1080p transport, a bounded tangent-space FOV margin, requested fixed render-resolution profile, per-run native log archival and atomic retention of still-fresh complete pairs. Age/present/origin gates remain unchanged. See [preview quality and continuity](vr-preview-quality.md).

All 94 VR tests pass. Actual mono HD +25% margin passed (6618 MiB peak); actual temporal 720p +25% margin produced accepted distinct pairs and closed normally. HD temporal stereo failed freshness and is not accepted for headset use. Diagnostics in the 720p run observed expired poses/images, including a maximum evaluated 701 ms pose that was rejected. The new visual result is still pending human validation. No new XR session was opened by the agent for this work.

Castellano: el usuario observó negros intermitentes en estéreo, poca nitidez en mono y bordes rectangulares en ambos. Se añadieron perfiles optativos HD/margen, conservación acotada de pares y registros por ejecución. Pasan 94 pruebas; mono HD y estéreo 720p con margen pasaron sin visor. Estéreo HD no pasó frescura y no se entrega como validado. Se detectaron poses/imágenes antiguas rechazadas; no se ampliaron los límites para ocultar el problema. Falta comprobar la nueva imagen en Quest.

Final quality build `748b4bb4`: 94 VR tests pass; inherited 357 passed / 5 known failures / 59 skipped in 341 seconds. Default-off startup passed at 3032 MiB, XR off; 19 stable hashes match. New private launchers separate mono HD + margin from temporal 720p + margin. Human retest requested only after build/source checks completed.

## Native stereo priority after failed human retest

The latest headset retest found no perceptible improvement. The accepted target is now complete same-state eye rendering, GPU-only OpenXR delivery and independently composed readable HUD/text. The old temporal capture path remains a diagnostic, not the target architecture. See [native stereo implementation gates](vr-native-stereo-gates.md) and the new `same_state_plan` output of the lineage analyzer. No new native stereo build or headset success is claimed.

Castellano: la última prueba no mostró mejora perceptible. La prioridad es la escena completa desde el mismo estado para ambos ojos, entrega GPU a OpenXR y HUD/textos independientes y legibles. El estéreo temporal sigue siendo diagnóstico. El nuevo plan de dependencias orienta la implementación; todavía no existe una nueva build estéreo nativa lista para el visor.

## Same-state GPU copy implementation

A new [GPU copy component](vr-same-state-gpu-copy.md) now records complete, matching same-state eye pairs without CPU pixel upload. The actual RTX 2060 SUPER smoke checked 4096 destination pixels in two completed submissions, exchanged eye colors and reused allocations. Negative fixtures reject mismatched producer state, pose, origin, recording and target contracts before recording. The original GPU smoke also passes. This is an offscreen transport component; game scene production and OpenXR submission are not connected yet. It does not change the playable temporal preview.

Castellano: implementada y probada en GPU real la copia conjunta de ojos con el mismo estado, sin subir píxeles desde CPU. Verificados 4096 píxeles en dos envíos, colores intercambiados y recursos reutilizados tras finalizar. Las discrepancias de estado/pose/origen/grabación/destino se rechazan sin comandos. Sigue pendiente conectar escena completa y OpenXR; no cambia todavía la prueba jugable.

## Native translated-view, transfer and HUD work

See [translated-view progress](vr-translated-view-progress.md). A real same-state D32 draw passed with translated views (0.01 diagnostic units), exact unchanged controls, complete source/payload validation and retired resources. Large diagnostic shifts failed and remain negative evidence. The color path now has a separate protocol and GPU-validated projection/viewport scalar gate. Texture transfers/clears are observed; copied camera coherence is validated once per comparison call; a game-free HUD fitting helper keeps the complete UI rectangle inside both eye frusta. Neither complete scene stereo nor the actual separate HUD/OpenXR composition is implemented. Headset testing waits for the user's availability.

Castellano: validado un dibujo D32 real con vistas trasladadas desde el mismo estado; no equivale a la escena completa. Nuevo modo de color con protocolo y constantes verificadas, cobertura adicional de transferencias, menor trabajo repetido de validación y cálculo geométrico del futuro HUD separado. La composición real de escena/HUD/OpenXR sigue pendiente. No se solicita el visor hasta que el usuario esté disponible.

Validation follow-up: translated depth draw passed; actual color remained invisible across the eight-attempt bound and is not accepted. All 99 VR tests pass; inherited 358 passed / 5 known failures / 59 skipped in 322 seconds. Stable installation: 19 hashes unchanged. No headset session opened. Next native blocker is safe camera alias routing through global-memory reads, followed by complete scene production and separate UI composition.

Castellano: profundidad trasladada validada; color invisible en ocho intentos, sin aceptarlo como éxito. 99 pruebas VR correctas; regresiones 358 correctas / 5 conocidas / 59 omitidas. 19 hashes estables intactos. Sin abrir XR. El siguiente bloqueo es redirigir los alias de cámara usados por memoria global antes de completar escena y UI separada.


## 2026-11-14 — GPU camera alias closure

The [private guest-root diagnostic](vr-gpu-root-aliases.md) now finds all full-page GPU aliases and verifies complete per-eye camera readback through copied roots. The first real-game run found four aliases per eye, checked every one of the 13,568 camera bytes and retired 50,391,584 bytes of private allocations, with exact unchanged controls and no XR session. Own GPU checks also verify zero-count indirect arguments for invalid aliases or corrupted payloads. Actual game draws do not yet consume the private root; this is the next integration gate, followed by full scene/HUD/OpenXR work.

Castellano: el diagnóstico ya descubre todos los alias completos de cámara en la tabla GPU. Primera prueba real: cuatro alias por ojo, 13.568 bytes completos coincidentes y 50.391.584 bytes privados liberados; controles exactos y sin XR. El guardia GPU bloquea dibujos indirectos cuando los alias o los datos fallan. Falta conectar la tabla privada a shaders del juego y completar escena, HUD y OpenXR. No se pide aún otra prueba en el visor.


## Global-read inventory and guarded command consumption

The [global-read effect inventory](vr-private-root-read-inventory.md) follows entry-call-graph reachability so unused writer helpers do not masquerade as executed writes. Native captures now distinguish global reads from writes. A fresh associated capture contains 23 effect candidates among 64 programs, with all native write flags available. Extra camera fields and complete alias semantics remain unreviewed; no global game draw is enabled.

The isolated translated depth experiment now actually consumes GPU-guarded indirect commands: first actual-game attempt passed, 1344 elements per eye, exact unchanged controls, full 13,568-byte cameras and retired resources. Private roots remain bound to the authored lookup only. All 103 VR tests pass. The user is unavailable; no XR session is opened and no new headset test is requested.

Castellano: el inventario distingue auxiliares sin ejecutar de escrituras alcanzables y separa lecturas/escrituras globales en los metadatos del motor. Captura real: 23 candidatos entre 64 programas, todos con marcas nativas disponibles. Continúan pendientes campos adicionales y alias; no se habilitan dibujos globales. La profundidad trasladada ya consume comandos indirectos protegidos: prueba real correcta al primer intento, 1344 elementos por ojo, referencias exactas y cámaras completas. Pasan 103 pruebas VR. No se abre XR ni se pide prueba al usuario.


## Completed GPU camera payloads and controlled views

The [full camera payload audit](vr-camera-payload-audit.md) preserves every source word after GPU completion and byte-exact before/after comparison. Three isolated game runs passed root, indirect and depth controls and produced three distinct camera views. Motion ruled out several stationary matrix matches; remaining offsets are algebraic candidates only. No new camera field is patched or global draw enabled. All 105 VR tests pass; inherited 358 passed / 5 known failures / 59 skipped. Raw payloads remain local. No headset session is requested while the user is unavailable.

Castellano: captura completa después de finalizar GPU y comprobar la fuente intacta. Tres pruebas aisladas correctas con vistas distintas permiten descartar coincidencias engañosas en reposo. Los campos restantes son candidatos algebraicos, sin modificar ni habilitar nuevos dibujos globales. 105 pruebas VR correctas; regresiones 358 / 5 conocidas / 59 omitidas. Datos crudos locales y sin pedir el visor.


## Same-source GPU depth reconstruction

The [same-source depth pair](vr-same-source-depth-pair.md) reconstructs both eyes from one color/depth packet and composes the complete pair on GPU. The actual RTX 2060 SUPER test checks 32,768 pixels across four reused submissions, both packed formats, a rotated plane and invalid depth. All 106 VR tests pass. This is an offscreen authored-data component; native geometry stereo, a live immutable game source and OpenXR integration remain pending. No headset session is opened.

Castellano: nuevo productor de ambos ojos desde un solo paquete color/profundidad, con copia conjunta GPU. Comprobados 32.768 píxeles, cuatro envíos reutilizados, dos formatos, rotación y profundidad inválida. Pasan 106 pruebas VR. Sigue sin conectar al juego en directo ni a OpenXR y no equivale a renderizar toda la geometría por ojo.


## Frozen actual-scene depth pair

The GPU producer now processes a private real 1152×648 color/depth packet and produces a complete offscreen pair. Accepted pixels: 99.314% / 99.286%; original GPU packet unchanged; 587,171 pixels differ between eyes. Local visual review confirms the scene and shows silhouette/depth-discontinuity holes. The file input is uploaded by the test fixture; live-game GPU delivery and OpenXR remain pending. Raw packets and images stay private. All 106 VR tests pass.

Castellano: pareja GPU desde captura real congelada, 1152×648 por ojo, aceptación 99,314% / 99,286%, fuente intacta y 587.171 píxeles distintos. La imagen local muestra huecos en discontinuidades; no equivale a calidad validada en visor. El siguiente enlace es juego→origen GPU inmutable→OpenXR. Archivo e imágenes privados; 106 pruebas VR correctas.
