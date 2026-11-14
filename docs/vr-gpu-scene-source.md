# Immutable live-game GPU source / Origen GPU inmutable del juego

## English

The opt-in native source owner copies reviewed HDR color and D32 depth images into one contiguous device-local packet at the reviewed upscaler boundary. It records both planes on the same native command buffer and publishes metadata only after the actual engine timeline completes. This is the source stage for the [same-source depth pair](vr-same-source-depth-pair.md); it does not yet submit reconstructed eyes to OpenXR.

Three source slots share a 64 MiB allocation budget with one optional diagnostic readback. Each allocation is capped at 16 MiB; accepted images are single-sample 2D, one layer/mip, at most 1920×1080. Camera projection/rigidity/coherence, texture generation, sampled packed-color view format and supported program keys are checked before recording. Original color/depth images are read, not rewritten.

A const frame lease carries a private lifetime token. Copies of the frame also retain that token. A slot detaches its old metadata before testing whether the token is unique, so concurrent weak observers cannot resurrect rewritten metadata. All three leased slots cause backpressure rather than an overwrite. Consumers must retain their lease until their own GPU use completes, including timeout/device-loss retention.

Acquisition checks device identity, source generation and a one-second age bound. XR acquisition additionally requires a current matching-origin mono head pose; menu, temporal-eye and unapproved synthetic poses are rejected. Device changes permanently abandon the old pool even if the handle value returns. Shutdown frees only completed, unborrowed storage; uncertain resources remain retained for process teardown. The feature is off by default.

### Isolated validation

After building current experimental sources:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -GpuSceneSourceProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

This mode requires its own run, a private save snapshot and at least 120 seconds. It cannot open XR or share another capture/probe mode. The launcher arms it after loading time and writes fresh outputs under ignored `work/game-run/`. The diagnostic copies one completed device packet to a separate host-visible buffer, waits for its completion and writes a private HSVRD2 file. **That inspection uses CPU readback; normal source publication does not.** Neither the source packet nor its report is a public artifact.

The receipt verifier requires at least three completed/published sources, zero reported failures, bounded allocations and an exact bounded packet header/length. Actual-source game-free tests exercise partial allocation/map failures, pre-completion rejection, immutable copied leases, backpressure, expired weak metadata, pose/origin scope and permanent device abandonment. These tests use a Vulkan transport double; actual GPU correctness needs the separate native run.

The [native same-device diagnostic](vr-live-gpu-depth-pair.md) now consumes live leases. The [continuous producer](vr-continuous-gpu-eyes.md) is now checked too. Still pending: direct OpenXR delivery, late head poses, separate HUD/text composition, scene-dependent disocclusion limits, scale calibration and headset performance/comfort. The playable temporal preview remains an earlier path with the user's reported flicker and boundaries.

## Castellano

El nuevo propietario nativo, desactivado por defecto, copia color HDR y profundidad D32 al mismo paquete privado en GPU. Ambas copias pertenecen al mismo comando del motor; solo publica la captura cuando su contador real confirma la finalización. Es el origen para reconstruir ambos ojos desde una captura común. Todavía no presenta esos ojos en OpenXR.

Hay tres reservas de origen. El presupuesto total, incluyendo una lectura de diagnóstico opcional, es de 64 MiB y cada reserva admite hasta 16 MiB. Se comprueban tamaño, formato, generación de texturas, cámara y programa de captura. Las imágenes originales se leen sin modificarlas.

Las referencias y sus copias conservan un token privado que impide reutilizar la memoria hasta que todos los consumidores terminen. Cuando las tres reservas están ocupadas se omite la captura nueva. La adquisición rechaza dispositivos, generaciones, antigüedad u orígenes de pose incorrectos; no acepta una imagen temporal de un solo ojo como escena mono compartida. Un cambio de dispositivo invalida definitivamente ese pool. Al cerrar se conserva toda memoria cuya finalización o propiedad sea incierta.

El comando anterior ejecuta el juego en la copia aislada y requiere partidas privadas. No abre el visor ni admite otros diagnósticos simultáneos. Después de la carga copia un paquete completado a memoria visible para CPU y genera un archivo privado de inspección. Esa lectura puntual pertenece al diagnóstico; la publicación normal del origen conserva los píxeles en GPU.

El verificador exige al menos tres capturas completadas/publicadas, ninguna incidencia registrada y un archivo con tamaño y cabecera válidos. Las pruebas del código real verifican fallos de reserva, sincronización, referencias/copias, presión de reservas, poses y cambios de dispositivo. La simulación Vulkan de esas pruebas no sustituye la ejecución real.

El [diagnóstico nativo](vr-live-gpu-depth-pair.md) ya consume referencias vivas. El [productor continuo](vr-continuous-gpu-eyes.md) también está contrastado. Faltan entrega a OpenXR, usar poses recientes, componer HUD/textos por separado y validar escala, huecos de profundidad, rendimiento y comodidad en Quest. La prueba temporal anterior conserva los fallos que observó el usuario; este componente por sí solo no los da por resueltos.


### Actual RTX 2060 SUPER check / Prueba real

The isolated native run completed and published 300 packets with zero reported failures. Three allocations used 17,915,904 bytes (two source buffers reused plus one diagnostic readback). A private 1152×648, packed-format-123 packet had the exact 5,972,100-byte file extent. The existing offscreen reconstructor then accepted 741,425 / 741,221 of 746,496 pixels from that file and left its full GPU source bit-exact. This second tool explicitly uploads the frozen file: it does **not** demonstrate a direct live-source-to-reconstructor connection. All 108 VR cases pass; 19 stable installation hashes match.

Castellano: 300 capturas completadas/publicadas, sin incidencias registradas. Tres reservas ocuparon 17.915.904 bytes: dos orígenes reutilizados y una descarga de diagnóstico. Archivo privado de 1152×648, formato 123 y 5.972.100 bytes exactos. El reconstructor externo aceptó 741.425 / 741.221 píxeles de 746.496 y conservó la fuente GPU intacta. Esa segunda herramienta sube un archivo congelado; no comprueba todavía el enlace directo con el origen vivo. Pasan 108 pruebas VR y coinciden los 19 hashes de la instalación estable.
