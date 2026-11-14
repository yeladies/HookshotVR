# XR-owned reconstruction / Reconstrucción desde OpenXR

## English

This opt-in candidate addresses three observed defects: silhouette gaps, source-frustum edge rejection and low angular source resolution. It also moves eye reconstruction off the game-present schedule and into the XR owner. It is not yet an accepted headset fix or complete geometry stereo.

Run the SDK-enabled isolated build with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-xr-owned-preview.ps1
```

Connect Quest through Virtual Desktop first. The guarded 180-second test loads a private save and arms scene capture after about 80 seconds. Menus/HUD remain pending. The stable installation and its saves are only read. This mode creates fresh eye transforms from current `xrLocateViews` poses; each pair borrows one completed immutable GPU scene packet, reconstructs both eyes, copies to the acquired swapchain image and retains the packet until its fence completes. The paired projection contains the actual reconstruction pose/FOV. No pixel staging/map/readback is used in this runtime route.

The Vulkan device must have explicitly enabled shaderInt64 and bufferDeviceAddress. A single reusable device-local output/parameter allocation stays below 16 MiB. Missing capabilities, partial initialization, pending image/fence waits and uncertain submission ownership have explicit rejection/retention tests. Old game-paced reconstruction remains available separately. Rendered-eye guard band is 5%, source tangent margin 25%; freshness/origin gates are preserved.

### Changes and limits

- The source and each eye are 1440x1440. This equals the 1920x1080 source pixel budget and adds 33% vertical source samples. Square host output requires the explicit VR environment flag and is capped at 1440; ordinary output admission is unchanged. Source allocations retain their 16 MiB and total pool budgets.
- The inverse solver no longer rejects a ray only because its direction at infinity lies outside the source image. It clamps the initial depth seed, then still requires the final finite-depth intersection to be inside. An independent GPU plane test proves recovery of genuinely available peripheral geometry and rejection of final out-of-coverage intersections.
- Optional radius-eight source-pixel background extrapolation repairs residual disocclusion holes using farther-depth neighbours. It only changes failed correspondences, does not modify valid output and does not fill invalid-depth/out-of-coverage failures. Repaired pixels remain classified as residual/approximate, not proved geometry. It cannot recover truly hidden surfaces or correct CPU-culling errors.
- Depth-aware bilinear color sampling is retained. Final game tone mapping, transparent effects, fully native per-eye geometry and HUD/menu integration remain separate work.

Khronos requires submitted projection metadata to describe the poses used for the image; its [frame-submission guide](https://github.com/KhronosGroup/OpenXR-Guide/blob/main/chapters/frame_submission.md) describes late pose sampling. This implementation reconstructs with the current sample rather than relabeling old eye images. CPU observation-age counters remain observation ages, not motion-to-photon latency.

### Evidence before headset testing

Native Windows SDK final build source `12739701` succeeded; the earlier square-source diagnostic used `dee9a833` before the device-generation retention fix. The VR suite has 156 passing full-suite cases, including the new host-square regression and device-generation retention case. Twenty XR-owner cases verify repeated reconstruction from one scene with changing XR poses, absent/stale/wrong sources, device capabilities, initialization errors, image/fence waits and conservative lease retention. Both direct-buffer and image GPU oracles pass 98,304 pixels / twelve submissions, including finite-depth peripheral recovery and bounded background repair. Inherited suite remains 358 passed / 5 known environment failures / 59 skipped.

The actual `GpuLatePair` allocation/pipeline/command recorder also passes a real-GPU authored-input test: two current pose jobs share one source state, 16,384 checked output pixels agree with independently built fixture cameras, 2,304 pixels change between poses and the source stays unchanged. This does not create an XR session or validate headset quality.

A 150-second non-XR game diagnostic produced 1,320 completed/published 1440-square pairs, zero producer failures and three changing output-only readbacks; actual world and camera resolution were 1440-square. This validates the square source/old producer, not the new XR-owned runtime presentation. A separate raw-source capture caught a flat transition frame and is not useful evidence for repair quality.

A controlled offscreen comparison on a previous complete private 1152x648 scene packet, with identical source/cameras/filter and runtime residual tolerance, reduced black pixels from 10,447 to 6,224: 4,223 residual holes filled, zero previously nonblack pixels changed. Remaining 6,224 pixels are out-of-coverage, and repaired color is approximate background extrapolation. Original GPU packet stayed byte-identical. This frozen-file experiment explicitly uploads the source and reads outputs for inspection; it is separate from the runtime GPU-only path. Source captures, matrices, images and binaries remain private under work/. Real-headset quality/performance acceptance remains pending.

## Castellano

Esta candidata opcional trata los huecos de silueta, un rechazo incorrecto de geometría lateral y la baja resolución angular. También reconstruye los ojos desde el propietario OpenXR con las poses actuales, en lugar de esperar al siguiente fotograma del juego. Todavía requiere prueba visual; no es estéreo de geometría completa.

Ejecuta el comando anterior con las Quest conectadas por Virtual Desktop. La prueba dura tres minutos y carga una copia privada de la partida; la captura se activa hacia los 80 segundos. Se reconstruyen ambos ojos desde una misma escena GPU completa y protegida. Sus poses/FOV describen esa reconstrucción real. El préstamo de la fuente se mantiene hasta completar la GPU, incluidas las rutas de fallo. No se transportan píxeles por CPU en esta ruta. Menús/HUD siguen pendientes y la instalación estable solo se lee.

La fuente y cada ojo pasan a 1440x1440: los mismos píxeles totales que 1920x1080 y un 33% más de muestras verticales. El formato cuadrado necesita activación VR explícita y conserva los límites de memoria. El solver ya no descarta por adelantado rayos que sí alcanzan geometría cercana dentro del encuadre; la intersección final debe seguir dentro. La reparación de fondo está acotada a ocho píxeles de origen y solo actúa sobre correspondencias fallidas: no altera píxeles válidos, no rellena profundidad inválida ni los límites sin cobertura. Es extrapolación aproximada, no recuperación de superficies ocultas.

Pasan 156 comprobaciones VR, la compilación SDK, los oráculos GPU de 98.304 píxeles/doce envíos en ambas rutas y las comprobaciones del proyecto original mantienen 358 correctas, cinco fallos conocidos de entorno y 59 omitidas. La prueba real sin XR completó 1.320 pares cuadrados, tres muestras cambiantes y cero fallos de producción. Esto prueba la fuente cuadrada; no valida todavía la presentación nueva desde XR.

En una captura privada completa anterior, manteniendo fuente, cámaras y muestreo, los píxeles negros bajan de 10.447 a 6.224: 4.223 huecos reparados y ningún píxel previamente visible modificado. Los restantes quedan fuera de cobertura. Una captura nueva de transición plana se descartó para valorar calidad. Estos experimentos de fichero usan lectura/subida explícita para inspección y no equivalen a transporte de píxeles por CPU en el juego. Vídeos, matrices, imágenes y binarios permanecen privados. La aceptación en las Quest y su rendimiento siguen pendientes.
