# GPU-pair projection metadata / Metadatos de proyección del par GPU

## English

A checked metadata builder now maps one immutable, completed GPU pair to two OpenXR projection views. Each view keeps the pose and asymmetric FOV used to render that eye and occupies its entire half of the output image. It does not paste newly located poses onto an older pair or derive headset FOV from the source image rectangle.

Admission requires the current LOCAL origin, matching device generation, a bounded complete pair, ordered/finite observation times, valid position and orientation for both eyes, supported eye extents and an explicitly acquired/waited target whose pair copy has completed. Temporal sources and source observations older than 250 ms are rejected. Output remains unchanged if either eye fails validation.

`GpuProjectionData` stores its views without an internal pointer. Calling `layer()` returns a projection-layer header pointing to that object's own array, so copying the metadata does not preserve a dangling pointer to a temporary builder. The object must remain alive through `xrEndFrame`.

This component performs no GPU or OpenXR calls. Completion/acquisition flags are owner contracts, not independent GPU evidence. The native continuous pair owner, real swapchain acquisition/copy/release, frame-loop wiring, UI and headset check remain pending. The test verifies truthful poses/FOV, complete left/right image rectangles, metadata-copy lifetime, expiry, recenter invalidation and rejection before partial output. No headset session is opened.

See the [direct GPU copy](vr-direct-buffer-presentation.md), [native source-to-eye diagnostic](vr-live-gpu-depth-pair.md) and [current checkpoint](vr-development-checkpoint.md).

## Castellano

El constructor de metadatos transforma un par GPU completo e inmutable en dos vistas de proyección OpenXR. Conserva la pose y el FOV asimétrico usados para generar cada ojo y usa todo su rectángulo de imagen. No asigna poses recién localizadas a una imagen anterior ni deduce el FOV del visor a partir del rectángulo de la fuente.

Exige origen LOCAL actual, generación de dispositivo coincidente, tiempos válidos/ordenados, posición y orientación válidas, límites de imagen y destino adquirido/esperado con copia GPU completada. Rechaza fuentes temporales, datos de más de 250 ms y cualquier ojo inválido sin modificar parcialmente la salida. Copiar los metadatos conserva correctamente la duración del array de vistas; el propietario debe mantenerlo vivo hasta `xrEndFrame`.

No llama a GPU ni a OpenXR. Las marcas de adquisición/finalización son contratos del propietario, no pruebas independientes. Faltan productor continuo, adquisición/copia/liberación del swapchain real, conexión al bucle de sesión, UI y prueba en visor. Las pruebas comprueban poses/FOV, rectángulos completos, duración de copias, caducidad y recentrado. No se ha abierto el visor.
