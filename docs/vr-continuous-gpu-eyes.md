# Continuous GPU eye stream / Flujo continuo de ojos GPU

## English

The native producer now publishes a continuous sequence of reconstructed stereo buffers from the live immutable HDR/D32 source. One source lease feeds both eye dispatches; no source pixels are downloaded/uploaded between game rendering and eye reconstruction. Normal output remains device-local RGBA8 UNORM. Final game tone mapping, HUD/text extraction and actual OpenXR presentation remain separate work.

Three output slots have private lifetime tokens. Const leases and copied frame values pin the complete paired buffer; weak metadata cannot resurrect a rewritten frame. If all slots are borrowed, the producer skips new work instead of overwriting them. A source lease survives until the engine timeline completes its two-eye compute job; only then is output published and the input lease released. Output consumers must keep their own lease through their GPU copy fence.

The producer preserves the common target pose sample and per-eye pose/FOV used for reconstruction. Real mode requires a running/focused/renderable XR mailbox, valid head/view flags, matching source origin/device generation and current mono source poses. Explicit probe mode uses a synthetic head/eye rig, refuses an active XR session and requires a synthetic source. Normal acquisition rejects synthetic output unless a diagnostic caller explicitly allows it. Recenter invalidates old-origin completion; observed device changes permanently retain uncertain ownership.

A separate 64 MiB budget covers three reusable output/parameter allocations and optional output readback. Each allocation is capped at 16 MiB; the current launcher uses 1024×1024 per eye. The compute shader has no normal-path atomic counters or CPU image readback. The probe takes only three completed output samples, separated by at least thirty completed pairs, and checks that their file digests differ. Those images and all reports remain private under ignored `work/`.

### How to run the isolated probe

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -GpuEyeStreamProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

The launcher supplies a ten-percent source tangent margin by default, private saves, mono synthetic camera motion and no XR session or old preview readback. It rejects competing probes/XR modes. Current-source/native receipts must match before launch. `-PreviewResolution 1080p` is an optional source-quality profile; this new continuous path has so far been measured with the default source profile, not that HD combination.

The receipt requires at least 120 completed/published pairs, three fresh complete PPMs with changing digests, active synthetic mono camera provenance, bounded allocations and zero recorded producer failures. SHA256 uses .NET directly because the available Windows PowerShell process does not expose `Get-FileHash`.

### Resolution-independent depth coherence

The first square-eye run exposed fine black stripes across otherwise valid surfaces. The shader measured nearest-source-texel quantization in target pixels, so changing from a 1152×648 source to square eyes could exceed its threshold even on a flat plane.

The final ray/depth check now measures disagreement in **source pixels**, after recomputing the intersection with the final sampled depth. The tolerance itself stays bounded; rays without source coverage, behind-eye surfaces and nonconverging depth remain rejected. The authored 80×40-source / 64×64-eye plane test fails with the old shader and passes after this change. An alternating near/far depth test still rejects oscillating lookups. The full GPU oracle now checks 49,152 pixels in six submissions through both direct-buffer and prior image-mediated paths, including both packed formats, rotation and invalid depth; complete source packets remain unchanged.

### Actual Windows / RTX 2060 SUPER evidence

The corrected isolated game run completed/published 2,190 pairs at 1024×1024 per eye, with zero failures, zero borrowed-slot skips and one rejected pose observation. Stream allocations used 25,166,336 bytes. Three output samples completed in three attempts; their pixels/digests differ, and the last sample has 867,875 distinct eye pixels. The old preview reported zero published images. No headset FPS or motion-to-photon measurement is claimed.

Local visual inspection shows that the quantization stripes are gone. An example before/after sample has 167,603 / 3,720 black pixels out of 2,097,152; the camera poses/times differ, so this is an observation rather than a controlled quality benchmark. Disocclusion outlines, source resolution, edge geometry/culling and final color still need work. This is source-depth reconstruction, not native replay of all geometry for each eye.

Actual-source game-free tests cover setup failures, immutable/copy/weak leases, three-slot backpressure, real/synthetic mailbox scope, stale output, shutdown with pending GPU work, device-handle abandonment, failed optional readback after new GPU work and recentering while old-origin jobs are pending. All 114 VR cases pass. The next integration is a real swapchain/session consumer using the checked [direct copy](vr-direct-buffer-presentation.md) and [projection metadata](vr-gpu-projection-metadata.md). No new headset test is requested while the user is unavailable.

## Castellano

El productor nativo ya publica un flujo continuo de parejas reconstruidas desde color/profundidad GPU del juego. Ambos ojos usan la misma referencia a la fuente, sin descargarla y volver a subirla. La salida normal es RGBA8 UNORM en memoria de dispositivo. Faltan tratamiento final de color del juego, HUD/textos y presentación OpenXR real.

Hay tres reservas con referencias protegidas: también las copias del objeto conservan la memoria, y retenerlas todas detiene nuevas capturas en vez de sobrescribirlas. La fuente se conserva hasta terminar el trabajo GPU de ambos ojos. Solo entonces se publica la pareja; quien la copie al visor tendrá que conservar su referencia hasta completar su propio envío.

Se conservan muestra de pose común y pose/FOV de cada ojo. El modo real exige sesión/muestras válidas y origen/generación coherentes. El diagnóstico usa cabeza y ojos sintéticos, rechaza una sesión XR activa y no puede entrar en la adquisición normal del visor. El recentrado descarta trabajos del origen anterior. Memoria máxima del propietario: 64 MiB, con reservas individuales de hasta 16 MiB y 1024×1024 por ojo en el lanzador actual.

El comando anterior carga una partida privada, amplía por defecto un diez por ciento las tangentes del FOV de origen y ejecuta movimiento sintético sin visor ni transporte de píxeles del preview antiguo. Descarga únicamente tres salidas separadas para inspección y comprueba que cambien. El perfil opcional `-PreviewResolution 1080p` aún no se ha medido con esta nueva combinación continua.

La primera ejecución mostró líneas negras: el criterio de coherencia medía la cuantización de la fuente en píxeles de destino. Ahora se mide en píxeles de origen y sigue rechazando profundidad incoherente, rayos fuera de cobertura y superficies detrás del ojo. Un plano con origen rectangular y ojos cuadrados reproduce el fallo anterior y verifica su corrección; profundidad alternante sigue rechazándose cuando no converge. Oráculo GPU: 49.152 píxeles en seis envíos por ambas rutas, fuente intacta.

Prueba real corregida: 2.190 parejas completadas/publicadas, cero incidencias, una observación de pose rechazada, 25.166.336 bytes del flujo y tres muestras distintas en tres intentos. Última muestra: 867.875 píxeles diferentes entre ojos. El preview antiguo publicó cero imágenes. La inspección local confirma que desaparece la rejilla; siguen pendientes huecos de profundidad, resolución, geometría en los límites y color. No equivale a renderizar toda la geometría por ojo ni a medir rendimiento en Quest.

Pasan 114 pruebas VR, incluidas referencias, presión de reservas, fallos y recentrado con trabajos pendientes. El siguiente paso es conectar el consumidor de swapchain/sesión con la copia directa y los metadatos ya comprobados. No se abre ni solicita el visor mientras el usuario está ausente.
