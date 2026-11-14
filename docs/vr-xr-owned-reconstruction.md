# XR-owned reconstruction / Reconstrucción desde OpenXR

## English

This opt-in candidate addresses three observed defects: silhouette gaps, source-frustum edge rejection and low angular source resolution. It also moves eye reconstruction off the game-present schedule and into the XR owner. The first headset retest improved contours and turn boundaries according to the user, but geometry coverage and sharpness remain inadequate. It is not complete geometry stereo.

Run the SDK-enabled isolated build with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-xr-owned-preview.ps1
```

Connect Quest through Virtual Desktop first. The guarded 180-second test loads a private save and arms scene capture after about 80 seconds. Menus/HUD remain pending. The stable installation and its saves are only read. This mode creates fresh eye transforms from current `xrLocateViews` poses; each pair borrows one completed immutable GPU scene packet, reconstructs both eyes, copies to the acquired swapchain image and retains the packet until its fence completes. The paired projection contains the actual reconstruction pose/FOV. No pixel staging/map/readback is used in this runtime route.

The Vulkan device must have explicitly enabled shaderInt64 and bufferDeviceAddress. A single reusable device-local output/parameter allocation stays below 32 MiB (29,491,456 bytes for the Sharp profile on this GPU). Missing capabilities, partial initialization, pending image/fence waits and uncertain submission ownership have explicit rejection/retention tests. Old game-paced reconstruction remains available separately. Rendered-eye guard band is now 8%, source tangent margin 30%; freshness/origin gates are preserved.

### Changes and limits

- Balanced uses 1440x1440 for the source and each eye; Sharp uses 1920x1920, 78% more pixels. Square host output requires explicit VR opt-in and is capped at 1920; ordinary output admission is unchanged. Source/diagnostic pair allocations are capped at 32 MiB each and their respective pools at 128 MiB. These caps do not change the ordinary renderer memory policy.
- The inverse solver no longer rejects a ray only because its direction at infinity lies outside the source image. It clamps the initial depth seed, then still requires the final finite-depth intersection to be inside. An independent GPU plane test proves recovery of genuinely available peripheral geometry and rejection of final out-of-coverage intersections.
- Optional radius-eight source-pixel background extrapolation repairs residual disocclusion holes using farther-depth neighbours. It only changes failed correspondences, does not modify valid output and does not fill invalid-depth/out-of-coverage failures. Repaired pixels remain classified as residual/approximate, not proved geometry. It cannot recover truly hidden surfaces or correct CPU-culling errors.
- Depth-aware bilinear color sampling is retained. Final game tone mapping, transparent effects, fully native per-eye geometry and HUD/menu integration remain separate work.

Khronos requires submitted projection metadata to describe the poses used for the image; its [frame-submission guide](https://github.com/KhronosGroup/OpenXR-Guide/blob/main/chapters/frame_submission.md) describes late pose sampling. This implementation reconstructs with the current sample rather than relabeling old eye images. CPU observation-age counters remain observation ages, not motion-to-photon latency.

### Latest headset feedback and follow-up candidate

The first real XR-owned run closed cleanly without XR API errors: 1,285 pair copies, 2,442 projection frames, source/eyes 1440-square and peak working set 6,153 MiB. The user reports improved contours and turn edges, but still sees incomplete geometry while moving the head and insufficient resolution. Mean CPU observation ages at successful `xrEndFrame` return were 20.59 ms for the reconstructed eye pose and 95.00 ms for the source pose. These are not motion-to-photon measurements.

The follow-up removes two demonstrated scheduling delays. The consumer polls the Vulkan timeline counter and publishes already completed source packets without waiting for the next game tick. No wait/flush or CPU pixel transfer is added. When an XR job retires, the owner may start its successor in that same XR iteration; one job remains pending at most and the output allocation is reused only after its fence. A regression test forces every short fence wait to defer and checks there is no idle iteration between completed jobs.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-xr-owned-preview.ps1 -Profile Sharp
```

Sharp raises actual source and eye dimensions to 1920-square; it is an explicit comparison profile because this adds GPU work on modest PCs. The latest isolated, non-XR game run produced 1,050 completed/published 1920-square pairs, three changing inspection samples, zero failures, an 88,474,112-byte diagnostic pool and 4,888 MiB peak working set. Actual world target and camera were both 1920-square. This diagnostic uses synthetic tracking and the old producer: it validates admission and data flow, not the new XR cadence or perceived sharpness.

The current native SDK build (`1af92253`) succeeds and all 157 VR cases pass. Inherited checks remain 358 passed / 5 known environment failures / 59 skipped. The actual XR-owner GPU component matches an independent fixture for 7,372,800 pixels at 1920 per eye using one authored source; a separate two-pose small-image run checks motion (2,304 changed pixels). Neither creates an XR session. The new cadence, Sharp performance and visual improvement still require a headset retest. Wider margins cannot recover geometry absent from the source; native per-eye rendering/culling remains necessary for full coverage.

### Earlier evidence before the first headset retest

Native Windows SDK final build source `12739701` succeeded; the earlier square-source diagnostic used `dee9a833` before the device-generation retention fix. The VR suite has 156 passing full-suite cases, including the new host-square regression and device-generation retention case. Twenty XR-owner cases verify repeated reconstruction from one scene with changing XR poses, absent/stale/wrong sources, device capabilities, initialization errors, image/fence waits and conservative lease retention. Both direct-buffer and image GPU oracles pass 98,304 pixels / twelve submissions, including finite-depth peripheral recovery and bounded background repair. Inherited suite remains 358 passed / 5 known environment failures / 59 skipped.

The actual `GpuLatePair` allocation/pipeline/command recorder also passes a real-GPU authored-input test: two current pose jobs share one source state, 16,384 checked output pixels agree with independently built fixture cameras, 2,304 pixels change between poses and the source stays unchanged. This does not create an XR session or validate headset quality.

A 150-second non-XR game diagnostic produced 1,320 completed/published 1440-square pairs, zero producer failures and three changing output-only readbacks; actual world and camera resolution were 1440-square. This validates the square source/old producer, not the new XR-owned runtime presentation. A separate raw-source capture caught a flat transition frame and is not useful evidence for repair quality.

A controlled offscreen comparison on a previous complete private 1152x648 scene packet, with identical source/cameras/filter and runtime residual tolerance, reduced black pixels from 10,447 to 6,224: 4,223 residual holes filled, zero previously nonblack pixels changed. Remaining 6,224 pixels are out-of-coverage, and repaired color is approximate background extrapolation. Original GPU packet stayed byte-identical. This frozen-file experiment explicitly uploads the source and reads outputs for inspection; it is separate from the runtime GPU-only path. Source captures, matrices, images and binaries remain private under work/. The latest follow-up candidate still awaits real-headset quality/performance acceptance.

## Castellano

Esta candidata opcional trata los huecos de silueta, un rechazo incorrecto de geometría lateral y la baja resolución angular. También reconstruye los ojos desde el propietario OpenXR con las poses actuales, en lugar de esperar al siguiente fotograma del juego. La primera prueba en visor mejoró contornos y bordes según el usuario, pero siguen faltando geometría y nitidez. No es estéreo de geometría completa.

Ejecuta el comando anterior con las Quest conectadas por Virtual Desktop. La prueba dura tres minutos y carga una copia privada de la partida; la captura se activa hacia los 80 segundos. Se reconstruyen ambos ojos desde una misma escena GPU completa y protegida. Sus poses/FOV describen esa reconstrucción real. El préstamo de la fuente se mantiene hasta completar la GPU, incluidas las rutas de fallo. No se transportan píxeles por CPU en esta ruta. Menús/HUD siguen pendientes y la instalación estable solo se lee.

Balanced usa fuente y ojos de 1440x1440; Sharp sube ambos a 1920x1920, un 78% más de píxeles. El formato cuadrado necesita activación VR explícita. Cada reserva de fuente/par diagnóstico queda limitada a 32 MiB y cada pool a 128 MiB; no cambia la política de memoria del renderer ordinario. El solver ya no descarta por adelantado rayos que sí alcanzan geometría cercana dentro del encuadre; la intersección final debe seguir dentro. La reparación de fondo está acotada a ocho píxeles de origen y solo actúa sobre correspondencias fallidas: no altera píxeles válidos, no rellena profundidad inválida ni los límites sin cobertura. Es extrapolación aproximada, no recuperación de superficies ocultas.

En la candidata anterior pasaron 156 comprobaciones VR, la compilación SDK, los oráculos GPU de 98.304 píxeles/doce envíos en ambas rutas y las comprobaciones del proyecto original mantienen 358 correctas, cinco fallos conocidos de entorno y 59 omitidas. La prueba real sin XR completó 1.320 pares cuadrados, tres muestras cambiantes y cero fallos de producción. Esto prueba la fuente cuadrada; no valida todavía la presentación nueva desde XR.

En una captura privada completa anterior, manteniendo fuente, cámaras y muestreo, los píxeles negros bajan de 10.447 a 6.224: 4.223 huecos reparados y ningún píxel previamente visible modificado. Los restantes quedan fuera de cobertura. Una captura nueva de transición plana se descartó para valorar calidad. Estos experimentos de fichero usan lectura/subida explícita para inspección y no equivalen a transporte de píxeles por CPU en el juego. Vídeos, matrices, imágenes y binarios permanecen privados. La aceptación en las Quest y su rendimiento siguen pendientes.

### Resultado en visor y siguiente comparación

La primera sesión real de esta ruta terminó sin errores OpenXR: 1.285 copias de pares, 2.442 fotogramas de proyección y un máximo de 6.153 MiB. Mejoraron contornos y bordes al girar; siguen faltando geometría y resolución. La antigüedad media observada al devolver `xrEndFrame` fue de 20,59 ms para la pose reconstruida y 95,00 ms para la pose de la fuente. No son medidas de movimiento a imagen.

La nueva candidata publica fuentes ya terminadas consultando el contador de la GPU desde el consumidor, sin esperar al siguiente tick del juego. También inicia el trabajo siguiente en la misma iteración XR que retira el anterior. Mantiene un único trabajo pendiente, respeta su fence y no introduce esperas ni transporte de píxeles por CPU. Una prueba fuerza el aplazamiento de cada espera corta y detecta la antigua iteración vacía.

El comando anterior con `-Profile Sharp` activa 1920x1920 reales en fuente y ojos. Es un perfil de comparación opcional por su mayor coste GPU. El margen de tangentes de la fuente sube al 30% y el de los ojos al 8%, con metadatos de proyección coherentes. La prueba real sin XR produjo 1.050 pares, tres muestras cambiantes y cero fallos; confirmó objetivo y cámara de 1920x1920, pool diagnóstico de 88.474.112 bytes y máximo de 4.888 MiB. Usa seguimiento sintético y el productor antiguo; no demuestra la nueva cadencia XR.

La compilación SDK actual (`1af92253`) y las 157 pruebas VR pasan. La batería heredada conserva 358 correctas, cinco fallos conocidos de entorno y 59 omitidas. El componente GPU real del propietario XR coincide con la referencia independiente en 7.372.800 píxeles a 1920 por ojo; la comprobación separada de dos poses confirma 2.304 píxeles cambiantes. Falta medir la nueva cadencia y valorar nitidez, bordes y fluidez en Quest. Un margen mayor no recupera superficies que no existen en la fuente; sigue pendiente renderizar geometría y culling completos por ojo.
