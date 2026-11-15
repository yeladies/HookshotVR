# Camera rebuild / Reconstrucción de cámara

## English — 2026-11-15

Status: replacement camera foundation, **not a playable native-stereo build**. The user approved this rewrite after reviewing the mathematical diagnosis and retirement inventory. Do not launch the withdrawn headset profiles from this branch.

### Preserved baseline

- Branch `vr/archive-depth-camera-20261115`, commit `710ce5cc`, preserves the last depth-camera source, tests and docs, including the previously unpublished queue-pacing experiment.
- The last tested native artifacts, source `bef0705d` / ABI 20, and their hash receipt are also preserved privately under `work/archive-depth-camera-20261115/`. Never publish that directory or other game artifacts.
- The last headset feedback was “ha mejorado”; complete blackout resolution was not established. The last result had 312 post-first-image omitted layers, classified as stale source. This does not measure black pixels inside a submitted layer.

### Mathematical findings

The reviewed main-camera convention is RH, +Y up, -Z forward, row-major world-to-eye matrices multiplying column vectors. The private historical camera samples have determinant approximately +1. There is no demonstrated Y/Z swap or handedness reversal to fix.

The withdrawn path composed a relative tracked head transform over a continuously updated body/game camera. It used the entire initial HMD orientation as its reference and a provisional one-game-unit-per-metre conversion. This retained a dependency on game camera motion and canceled initial head pitch/roll against a game-camera reference. It was not an independent Link/head rig.

Physical world scale remains uncalibrated. Original host comments describe character-controller speeds in metres/second (run about 4.5), supporting a possible one-unit/metre interpretation; comments and apparent visual scale are not a measured calibration. Do not claim that a proven unit-factor error caused the reported judder.

The old game source used one symmetrically widened perspective. Both reconstructed eyes shared a source state, but did not contain separately rendered eye geometry. No amount of source FOV can recover hidden surfaces.

Native NVN projection uses -1..1 depth. Existing vertex/mesh translation maps depth into Vulkan's 0..1 range, and the native viewport has negative height for Y. The new native adapter therefore supplies -1..1, +Y projections; a separate direct-Vulkan consumer must explicitly choose its own one-time depth/Y adaptation.

### Implemented foundation

`vr/src/vr_camera.c` / `.h` is C99 with no game pointers, OpenXR/Vulkan calls, clock reads, smoothing, game-camera base or mutable retained camera state. A caller provides:

- One rig: world anchor, yaw, tracking reference in metres, positive explicit game-units/metre, and origin generation.
- One located frame: sequence, opaque predicted XR time, head pose, both absolute eye poses and four FOV angles per eye.
- Native near/far, clipping convention, depth order and explicit Y conversion ownership.

Each call rebuilds absolute world-from-eye, view, asymmetric projection, VP and world-space frustum planes. Translation is `rig_position + scale * rig_yaw * (located_position - tracking_reference)`; orientation is `rig_yaw * located_orientation`. The eye poses already contain IPD; no extra eye separation is invented. `head_from_eye` is diagnostic tracking-space hierarchy data and is not reapplied. The rig preserves gravity alignment while retaining the initial HMD pitch/roll.

The module supports finite/infinite far planes, both clip-depth conventions, forward/reversed depth and conservative stereo-union AABB visibility. Invalid inputs leave the complete output unchanged. Generation mismatch rejects a frame; recenter policy belongs to the rig owner. There is no old one-metre translation cutoff; room collision remains separate unfinished work.

`gpu/vk/vk_xr_camera_frame.cpp` adapts one immutable renderer snapshot into that C contract. It copies real per-eye FOVs, maps position/orientation validity separately, preserves frame/time identity and explicitly selects the reviewed NVN depth/Y convention. It does not read a previous game matrix, calibrate world scale, override a game controller or submit game stereo.

### Retirement and frame loop

The VR additions in `host/hos_main.c`, `host/nn_applet.c` and `nx2/oot_wide.py` were restored to the supported Hookshot base. The host's `rx_xr_head_camera` and shared `hos_vr_*` lens/culling globals were removed. The native renderer's ABI 20 head/lens slots remain present and null, preserving structure layout. A future ABI extension must advance the version.

The original guarded free camera, ultrawide, HUD and movie behavior remain. Original view injection site `f_804e48c0` / `L_804e5068` is Hookshot's debug hook, not proof of an early controller/visibility boundary. No new primary-camera writer has been enabled there.

Production rejects the withdrawn immersive, temporal, wide-lens and reconstructed-world environment modes. Their launchers fail before accessing an installation. Legacy math/ownership fixtures remain available only as reference tests; `HOOKSHOT_VR_LEGACY_FIXTURES` is passed by their CMake targets, never by the native game builder. Three tests requiring removed game writers/admission were replaced by retirement checks, rather than representing those features as implemented.

The OpenXR worker now publishes the located head/eye snapshot before preview work and `xrEndFrame`. Its CPU observation timestamp is taken at that publication, not refreshed at completion. A recenter immediately invalidates the previous mailbox generation. A blocked-EndFrame fake-runtime test proves the new C adapter can consume the frame before EndFrame returns, without a real headset.

### Remaining integration gates

1. Identify all selected gameplay-camera controller writers and a boundary before CPU visibility. Give VR exclusive authority there over interpolation, recentering, shake and dynamic FOV, while preserving other cameras and UI behavior.
2. Read Link's guarded root position independently of animation bob and the old camera; define explicit locomotion yaw and a reproducible world-scale calibration. Do not infer physical scale from projection near/far alone.
3. Read authoritative native near/far and build coherent center/union visibility for the two eye frusta. The C union helper is available; the game's actual culling consumers are not yet wired to it.
4. Render the complete eye-dependent scene twice from one simulation state. Audit hidden-pointer camera copies, compute/mesh visibility, indirect arguments, aliases, shadows, transparent effects and per-eye histories. Replaying one isolated draw does not establish complete scene replay.
5. Submit native eye images with the corresponding located poses/FOVs and a coordinated render deadline. The old stale mono-source reconstruction is not a fallback for this route.
6. Add readable HUD/menu composition, verify game-state transitions offscreen, then request a useful Quest 3/VDXR test. No new headset test is ready at this foundation stage.

Native game rebuilding requires regeneration: removing lifted perspective hooks while using `-SkipLift` would leave calls to removed host writers in old generated C. The builder now compiles the C camera separately and includes its source directory in receipt fingerprints. The maintenance manifest tracks the new integration files.

### Validation

The replacement C checks cover asymmetric near-plane corners, near/far depth endpoints in all supported conventions, one-time IPD/scale, yaw/pitch/roll, absolute repeatability, large anchors, both-eye visibility and atomic invalid-input rejection. C++ adapter checks exercise actual renderer snapshot conversion. Production retirement checks and the blocked-EndFrame test require no game or headset.

All 162 applicable VR tests pass (22.28 seconds), including retained historical GPU ownership/reference fixtures. This establishes contracts, not full-scene native stereo or visual acceptance. Full native and inherited checks are recorded separately when complete. Stable-installation hashes must remain unchanged.

## Castellano

Estado: base nueva de cámara, **todavía sin build jugable con estéreo nativo**. El usuario confirmó la reconstrucción después de revisar el diagnóstico y el inventario. Los lanzadores antiguos del visor están retirados en esta rama.

Se conserva el código anterior en `vr/archive-depth-camera-20261115`, commit `710ce5cc`. Los binarios probados `bef0705d`, ABI 20, y sus recibos quedan privados en `work/archive-depth-camera-20261115/`. No se publican archivos del juego. La última respuesta fue «ha mejorado»; no confirmó que hubieran desaparecido todos los apagones.

### Qué se ha corregido en la base

- Se creó `vr_camera.c/.h` en C99, sin memoria del juego, llamadas de runtime, interpolación ni dependencia de la cámara original.
- Rig y poses se suministran por separado. La escala es explícita; se conserva pitch/roll inicial. Cada ojo usa directamente su pose absoluta y los cuatro ángulos de OpenXR, sin añadir IPD otra vez.
- Se producen vista, proyección asimétrica, VP, inversas rígidas y planos de visibilidad. La consulta de cajas usa la unión conservadora de ambos ojos. Un error deja intacta toda la salida.
- El adaptador nativo usa la convención NVN -1..1 y +Y. El traductor y el viewport de Hookshot ya hacen las conversiones a Vulkan; no se duplican.
- Se revirtieron los escritores VR de vista/lente y la admisión cuadrada experimental. Se conservan cámara libre, ultrawide y UI originales. Los callbacks antiguos del ABI quedan nulos.
- Las poses se publican antes del trabajo gráfico y de `xrEndFrame`, conservando su marca temporal. Una prueba bloquea EndFrame deliberadamente y comprueba que la cámara nueva ya recibe el fotograma.
- Los lanzadores y las opciones de entorno antiguas rechazan la activación. Las pruebas históricas conservadas no equivalen a habilitar esa cámara en el juego.

No hay evidencia de un fallo de mano derecha/izquierda ni de un intercambio Y/Z: la cámara auditada coincide con OpenXR. Tampoco se ha demostrado el factor físico real de unidades/metro. Los comentarios del controlador original hablan de m/s, pero la calibración sigue pendiente; no se atribuyen los tirones a una escala supuestamente errónea sin medirla.

### Trabajo pendiente antes de otra prueba en Quest

1. Auditar los escritores del controlador y tomar autoridad sobre la cámara principal antes de calcular visibilidad: sin suavizado, autocentrado, vibración o FOV dinámico del juego.
2. Anclar el rig a la raíz comprobada de Link, definir yaw de locomoción y calibrar escala sin usar la cámara corporal ni el movimiento de los huesos.
3. Conectar near/far originales y todos los descartes CPU/GPU a la vista y al volumen que cubre ambos ojos.
4. Renderizar la escena completa por ojo desde una sola simulación, con alias, trabajo compute/mesh, indirectos y historiales correctamente separados. La antigua copia de un dibujo aislado no resuelve esta fase.
5. Presentar las imágenes nativas con sus poses, completar HUD/menús y validar transiciones; después preparar una prueba útil en visor.

Pasan las 162 pruebas VR aplicables (22,28 s), incluidas las referencias históricas de propiedad GPU. Son comprobaciones de contratos, no aceptación visual del juego. La compilación nativa requiere regenerar el código para retirar los hooks antiguos. La instalación estable permanece separada.

### Native foundation verification / Comprobación nativa de la base

Native SDK source `5a9a437a` regenerated successfully and compiled with zero failures (97 seconds plus a 6-second host link). The generated C no longer references the retired VR lens/frustum writers. The guarded debug view hook remains the original Hookshot hook. This build is the normal game plus inactive replacement-camera infrastructure, not native game stereo.

A 20-second isolated default-off run activated no XR session, negotiation, immersive view or GPU world reconstruction; it reached its time limit normally, peak working set 3042 MiB. A separate 150-second non-XR root/camera diagnostic with a 4:3 picture verified main-camera/submission matching, the private root-alias diagnostic and completed camera payload export; peak working set 6647 MiB.

Combining that export with three earlier private views gives two distinct projection reference values. Three projection copies and one inverse-projection candidate still match across the lens change. These are algebraic candidates, not proof of history semantics or shader consumption; `safe_to_patch` remains false. All payloads, disassembly, images and generated code remain private. This diagnostic did not use the new camera to render the game.

La build SDK `5a9a437a` se regeneró y compiló sin fallos: 97 s de compilación y 6 s de enlace del host. El código generado ya no llama a los escritores VR antiguos. El arranque aislado de 20 s mantuvo todas las rutas VR desactivadas. La prueba privada de cámara/raíces de 150 s, con proporción 4:3 y sin XR, pasó y exportó el bloque de cámara tras completar GPU. La comparación con tres vistas anteriores aporta dos ópticas distintas, pero las coincidencias siguen siendo candidatas algebraicas: no se parchean campos ni se presenta estéreo nativo. Los datos del juego permanecen privados.

### Final checks / Comprobaciones finales

The complete inherited suite finishes with **358 passed, five known environment failures, 59 skipped** (163 seconds). Remaining failures are the existing FP include fixture, two .NET Builder fixtures, Windows symlink permissions and the VRAM-row warning-as-error fixture. The new C build dependency fixture passes all 11 cases, including Windows/Linux object invalidation. All 162 VR cases pass. Nineteen stable-installation manifest hashes remain unchanged after the native runs. No new headset session was opened.

La batería heredada final da **358 correctas, cinco fallos conocidos del entorno y 59 omitidas** (163 s). Pasan los 11 casos de dependencias de compilación y las 162 pruebas VR. Los 19 hashes de la instalación estable siguen intactos tras las pruebas nativas. No se abrió una nueva sesión en el visor. Continúan pendientes la autoridad exclusiva sobre el controlador, el rig de Link y el renderizado completo por ojo.
