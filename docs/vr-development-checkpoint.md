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
