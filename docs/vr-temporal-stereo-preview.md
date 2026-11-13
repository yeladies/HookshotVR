# Wide FOV and temporal stereo / FOV amplio y estéreo temporal

## English

This experiment targets the window-like view reported during the first mono headset test. It changes the real game's perspective lens and provides genuinely different source views from tracked left/right eye poses. **The stereo is temporal:** each eye is captured on nearby ordinary game frames, not twice from the same simulation snapshot. It does not implement the planned complete same-frame native stereo replay path. Moving objects, camera transitions and temporal effects can show mismatches or ghosting. This distinction must remain explicit in documentation and reports.

### Implementation

ABI20 adds a tracked lens callback and passes the game present identity to the camera callback. A symmetric perspective envelope covers both runtime eye FOVs. The engine field stores `tan(vertical_fov/2)`, not radians; the host converts and validates that value. Four original-instruction-checked perspective construction sites update both the matrix input registers and the stored FOV/aspect fields read by the CPU frustum. The existing ratio-only and orthographic/UI paths remain separate. The opt-in guard accepts the reviewed 16:9 perspective family or the current target ratio; other projections are preserved. The generated game code must be lifted again after this source change. Do not patch generated files manually.

The head centre supplies the tracking anchor. The actual runtime eye pose already contains its offset; no second IPD is added. Each eye phase spans two ordinary game presents so the existing bounded every-second-present readback can capture both eyes without doubling game updates or replaying compute writes. Both eye images carry their own source pose, FOV, origin generation and present. The feed rejects incomplete, duplicate, reversed, stale or distant pairs and clears eye history on a menu/mono transition. An owned 2560×720 swapchain uses two independent 1280×720 subrectangles and two correctly offset GPU copy regions.

The prior headset test confirmed smooth mono gameplay, head response and original controls. The user reported a narrow moving window, black menus and no sound. The preceding test intentionally muted audio. UI-only presents now have an explicit 2D path, but the real menu-open interval still produced no newly accepted menu image. Menu coverage is incomplete; this must not be reported as fixed. 3D frames lacking camera provenance still cannot masquerade as menus. Audio can be requested for the experimental process without changing Windows' default device. New headset perception, audio routing and full menu coverage require human checks.

### Prepared local commands

Once the build has passed its local checks:

```powershell
# Compare only the perspective change, with the same image in both eyes:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -WideFov -Audio -Seconds 180
# Enable two distinct, temporally captured eye views:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -TemporalStereo -Audio -Seconds 180
```

The development PC has private launchers in `work/headset-stereo-preview/`. Preparation can take about 90 seconds; the whole run is 180 seconds and closes automatically. Start with a still scene and small head movements. The camera remains third person; there is no head collision or physical weapon/controller implementation. Wider coverage can reduce apparent sharpness at the current1280×720 per-eye capture size. Remove the headset if uncomfortable.

Offscreen synthetic-pose validation, never mixed with XR:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -ImmersiveReadbackTest -TemporalStereo -SnapshotStableSaves -LoadTestSave -Seconds 150
```

Validation must establish both eye identities, different accepted images, projection FOV matching the requested lens, source/frame association, default-off behavior and stable-installation integrity. Game-free fixtures test the generated lens patch, eye offsets, pair policy and actual projection upload/subrectangles against a fake runtime. Actual game, regression and headset outcomes must be recorded separately. All generated code, binaries, captures and saves stay private under `work/`.

## Castellano

Este experimento aborda la vista de ventana observada en la primera prueba mono. Cambia la lente de perspectiva real del juego y obtiene imágenes distintas desde las poses izquierda y derecha del visor. **La estereoscopía es temporal:** cada ojo se captura en fotogramas cercanos del juego. No es el render de ambos ojos desde una misma instantánea de simulación. Los objetos en movimiento, transiciones e historial temporal pueden producir diferencias o dobles contornos. El render nativo completo de ambos ojos en el mismo fotograma sigue pendiente.

La interfaz sube a ABI20. El FOV cubre ambos campos de visión del runtime. El campo del motor almacena `tan(FOV_vertical/2)`, no radianes; el host realiza y valida esa conversión. Cuatro sitios protegidos por comprobación de instrucciones actualizan los registros de construcción de perspectiva y los campos de FOV/aspecto del frustum. No se amplía una captura por estiramiento. Hace falta regenerar el código del juego desde sus fuentes; nunca editar a mano el código generado. Los filtros conservan otras familias de proyección y el comportamiento normal con el modo desactivado.

El centro de la cabeza es el anclaje; la pose de cada ojo ya incluye su separación y no se añade otra IPD. Las dos imágenes conservan sus poses, FOV, origen y fotogramas. Se rechazan pares incompletos, repetidos, antiguos o demasiado separados. Cada imagen se copia a su región independiente de la swapchain. No se repite la simulación ni se duplican las escrituras de cómputo del mismo fotograma.

La prueba anterior confirmó fluidez mono, seguimiento y controles originales. Se observaron FOV estrecho, menús negros y falta de sonido; esa prueba llevaba el audio desactivado. Los fotogramas de interfaz sin escena 3D tienen una ruta 2D explícita, pero durante la apertura del menú real no se aceptó imagen nueva de menú. La cobertura sigue incompleta y no debe darse por corregida. Se mantiene el rechazo de imágenes 3D sin procedencia de cámara. El audio se solicita solo al proceso experimental. No se cambia el dispositivo predeterminado de Windows. Falta comprobar visualmente el nuevo FOV, la estereoscopía, los menús y el sonido en Quest.

Los comandos anteriores corresponden a `Probar-solo-FOV.cmd` y `Probar-fov-estereo.cmd` de la carpeta privada `work/headset-stereo-preview/`. Conecta Virtual Desktop antes de ejecutarlos. La preparación puede tardar unos 90 segundos y la prueba completa dura 180. Empieza con una escena quieta y movimientos suaves. Conserva tercera persona y controles convencionales; no hay colisión de cabeza ni armas/mandos VR físicos. Con1280×720 por ojo, ampliar el FOV puede reducir la nitidez aparente. Quita el visor si hay malestar.

Las pruebas automáticas, el ensayo real sin visor y la comprobación humana se registran por separado. El paquete está verificado sin visor; la comprobación visual en Quest sigue pendiente. Datos del juego, código generado, binarios, capturas y partidas permanecen privados en `work/`.

## Verified local build / Build local verificada

2026-11-13, Windows, RTX 2060 SUPER, 16 GB RAM, ABI20, native source `c5617507`.
The actual game/offscreen check passed: requested and observed vertical FOV1.74533rad (100°), horizontal FOV1.91986rad (110°), distinct accepted left/right image pairs, both tracked-eye identities and normal producer cleanup. Synthetic lens values exercise the same runtime-driven path; the headset run derives its target from actual runtime eye FOVs. This is not a measurement of headset FPS or latency. The process peaked at 6632 MiB.

All 88 VR tests pass. Inherited regression result:357 passed, 5 known baseline/environment failures, 59 skipped in 317 seconds. Default-off startup passed at 3005 MiB peak; all 19 monitored stable-installation hashes match. The real menu check returned `menu_transition_verified=false`; closing it restored camera-associated game images. Audio transport to Quest has not been verified. No new XR session was opened while implementing/testing this mode.

El ensayo real sin visor pasó con fuente `c5617507`, ABI20: FOV vertical solicitado y observado de 100° y horizontal de 110°, imágenes distintas y correctamente asociadas a ambos ojos, y cierre normal. Los ángulos sintéticos prueban la misma ruta que obtendrá el FOV de los ojos del runtime en Quest; no son una medición del FOV físico, FPS o latencia del visor. Pico de memoria: 6632 MiB.

Pasan 88 pruebas VR. Regresiones heredadas:357 correctas, los 5 fallos conocidos y 59 omitidas. Arranque ordinario correcto y 19 hashes estables intactos. La prueba del menú no pasó; cerrarlo recuperó las imágenes del juego. El sonido en Quest sigue pendiente de comprobar. La nueva versión todavía requiere su primera prueba visual en el visor.

## Private remote inspection / Inspección privada en remoto

The offscreen real-game check can save a bounded set of accepted eye images:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/tests/run_stereo_game_check.ps1 -Seconds 180 -CaptureImages
```

This never opens XR. It uses synthetic head poses and writes at most three left/right PPM pairs, their source metadata, and one later UI image into a fresh direct child of `work/`. Captures are game-derived private data: do not commit or upload them. PNG conversion must preserve the original pixels. These are source eye views, not photographs through a headset, and do not validate perceived depth, comfort, audio or headset frame rate. Both eye images come from nearby simulation frames.

La prueba anterior nunca abre XR. Usa poses sintéticas y guarda como máximo tres pares de ojos en PPM, sus metadatos y una imagen posterior de interfaz, en una carpeta nueva de `work/`. Son datos privados derivados del juego: no se publican ni se incluyen en Git. Convertirlos a PNG conserva los píxeles. Son las vistas fuente de cada ojo; no son fotos del visor ni validan profundidad percibida, comodidad, audio o rendimiento en Quest. Cada ojo procede de un fotograma cercano.

The first remote inspection produced distinct source-eye images at 110° horizontal / 100° vertical FOV and a visible inventory image. The continuous-menu transition gate still failed: one accepted UI image is insufficient proof of a stable menu. Main-aspect 3D frames without camera provenance remain suppressed. Edge geometry visible in the wide view also needs culling review; headset acceptance remains pending.

La primera inspección remota obtuvo imágenes distintas a 110° horizontal / 100° vertical y un inventario visible. El criterio de transición continua del menú siguió fallando: una sola imagen de interfaz no demuestra estabilidad. Se siguen rechazando escenas 3D sin procedencia de cámara. La geometría visible en los bordes de la vista amplia también requiere revisar el culling; falta la prueba en Quest.
