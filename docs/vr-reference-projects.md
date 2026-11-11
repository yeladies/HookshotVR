# VR implementation references

## oot-vr — retained for physical interaction work

User-supplied reference: https://github.com/gibranfelix/oot-vr
Reviewed reference revision: `f2cf5ec9d63bb264d55c548cf628ad13f64df462` (2026-11-12).

This is a standalone Quest adaptation of the classic game, based on Ship of Harkinian.
Hookshot instead recompiles a different game build and uses an NVN-to-Vulkan renderer.
The reference is useful for interaction design; its engine hooks and offsets are not interchangeable.

Start future mechanics research at:

- `docs/architecture.md`: integration map and documented limitations.
- `port/soh/soh/Enhancements/vr-combat/`: physical weapon and item interactions.
- `port/libultraship/src/fast/vr_physics.cpp`: hand collision representation.
- `port/libultraship/src/fast/vr_interface.cpp` and `include/vr_interface.h`: game/VR boundary.
- `port/libultraship/src/fast/vr_menu_input.cpp`: filtering game input while a VR menu is open.
- `port/libultraship/src/fast/vr_openxr.cpp`: session and pose handling.

Our later research should compare swing thresholds, swept weapon collision, one-hit-per-swing
rules, physical shield direction, two-hand aiming, thrown items, body calibration and menu focus.
Each mechanic must connect to Hookshot's authoritative gameplay calculations, not just its visible model.
No mechanics code has been copied, and no claim is made that these systems will transfer unchanged.

### Licensing boundary

The repository's `LICENSE` applies MIT to its own contributions with a stated scope.
`NOTICE.md` distinguishes inherited code, including areas without a declared license.
Do not treat the whole tree as MIT or import entire game files. Before reusing a specific
implementation, examine its history, provenance and applicable notices. Prefer independently
implementing the needed behavior against Hookshot's own interfaces when provenance is unclear.

Primary references:
[architecture](https://github.com/gibranfelix/oot-vr/blob/f2cf5ec9d63bb264d55c548cf628ad13f64df462/docs/architecture.md),
[license](https://github.com/gibranfelix/oot-vr/blob/f2cf5ec9d63bb264d55c548cf628ad13f64df462/LICENSE),
[notices](https://github.com/gibranfelix/oot-vr/blob/f2cf5ec9d63bb264d55c548cf628ad13f64df462/NOTICE.md).
