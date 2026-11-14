# Live GPU depth pair / Pareja de ojos desde GPU viva

## English

The native diagnostic now feeds the authored depth reconstructor directly from a completed [immutable game source lease](vr-gpu-scene-source.md). Both eye dispatches use that one device address and one captured state. The source is not downloaded and uploaded again. The compute kernel and parameter contract are shared with the independently checked offscreen GPU oracle; their authoritative headers now live under `gpu/vk/` so native build fingerprints cover them.

The diagnostic executes only at the two reviewed upscaler boundaries, before the original compute pipeline/descriptors/push constants are bound. It changes no source descriptors and reads original color/depth without writing them. Source device, generation, dimensions, queue family, completion, recording order, age and disjoint device-address ranges must agree. One retained lease covers both eye dispatches and final diagnostic output copy.

One reusable output pair, two parameter blocks, counters and completed-output readback fit within a separate 40 MiB budget; each allocation is capped at 16 MiB. Shutdown releases only known-completed work and keeps the source lease and all consumer resources when GPU completion or device ownership is uncertain. A changed device is permanently retained even if its handle value returns.

### Private diagnostic how-to

Build current experimental sources first, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -GpuDepthPairProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

This is a non-XR run with private saves. It captures three successful pairs from strictly advancing source states, reusing its allocations. Completed eye output is read back for pixel/counter validation and one private PPM. **This output inspection uses CPU readback; the source-to-eye reconstruction has no intermediate CPU pixels.** Original images, packets and logs remain under ignored `work/`.

An initial run passed transport counters but its final image was nearly uniform background. The diagnostic now also requires visible channel range and distinct eye pixels; it retries at most twelve attempts with a 250 ms delay after a background rejection. This is a bounded diagnostic content check for the selected test scene, not a general gameplay/visibility heuristic and not proof of correct stereo perception.

For a separate source/parameter comparison only, add `-GpuDepthPairSourceDebug`. This explicitly adds a host-visible copy of the same retained source and both parameter blocks. The report identifies source validation readback, and byte-compares completed GPU parameters with the submitted CPU metadata. It writes a private HSVRD2 source file. This option is not part of the ordinary no-source-readback test and can hit the budget at larger dimensions.

A comparison run produced the complete 1152×648 scene in both eyes with matching uploaded parameters. Its final native counters accepted 741,390 / 741,155 of 746,496 pixels; the same source processed externally gave equivalent imagery with slightly different iteration/residual settings. Neither result establishes physically correct disocclusions, calibrated scale or headset image quality. Source holes and transparent/depth mismatches remain visible.

### Late head-pose preparation

A separate metadata helper computes source-camera-from-eye as `R_capture^T * R_eye` and `R_capture^T * (p_eye - p_capture)`. Its tests compare the result against the existing engine camera override using a nonidentity game view and XR anchor, translated/rotated head poses, physically separated/canted eyes and asymmetric lenses. Invalid flags, crossed origins, temporal/menu sources, stale/future observations, invalid quaternions and large jumps are rejected without partially changing either eye. The helper is **not yet wired to the live native diagnostic or OpenXR**.

The native diagnostic still uses fixed ±0.032 source-unit eye offsets and the source lens. It does not render all game geometry per eye, submit an OpenXR projection layer, extract HUD/text, calibrate world scale or fix the user's existing temporal headset preview. Next integration is a reusable GPU pair owner, current located OpenXR poses, direct swapchain copy and separate UI composition, followed by the user's headset check when available.

## Castellano

El diagnóstico nativo ya consume directamente una referencia a una captura GPU completada. Ambos ojos leen la misma dirección y el mismo estado; la fuente no se descarga para volver a subirla. El shader propio y sus contratos son compartidos con la prueba GPU externa y quedan cubiertos por la huella de compilación nativa.

Se comprueban dispositivo, generación, tamaño, cola, antigüedad, orden y rangos separados. La referencia a la fuente se conserva hasta completar ambos ojos y su copia de inspección. Las reservas se reutilizan con un límite de 40 MiB; el cierre conserva todos los recursos si existe trabajo pendiente o propiedad de dispositivo incierta.

El comando anterior usa la copia experimental y partidas privadas, sin abrir XR. Requiere tres parejas correctas desde estados distintos. Solo descarga la salida de los ojos para comprobarla y guardar un PPM privado. Una ejecución inicial mostró fondo casi uniforme pese a completar los contadores; ahora ese caso se rechaza y hay como máximo doce intentos, con pausa entre rechazos. Es una comprobación del diagnóstico, no una regla de renderizado para el juego.

La opción adicional `-GpuDepthPairSourceDebug` descarga expresamente también la misma fuente y los parámetros para compararlos. El informe lo declara; no debe confundirse con el modo normal sin descarga de fuente. Una ejecución con comparación mostró la escena completa a 1152×648, parámetros GPU exactos y 741.390 / 741.155 píxeles aceptados por ojo de 746.496. La herramienta externa con esa misma fuente da imagen equivalente usando ajustes de iteración/residuo algo distintos. Siguen existiendo huecos de profundidad y transparencia.

También está preparada la matemática de poses recientes: se ha contrastado con las cámaras del motor, incluida base/áncora girada, traslación, separación/cantado de ojos y lentes asimétricas. Rechaza poses cruzadas, antiguas o inválidas sin modificar parcialmente las dos salidas. Aún no está conectada al consumidor nativo ni a OpenXR.

La prueba nativa usa todavía separación fija sin calibrar y la lente de la fuente. Faltan presentación OpenXR, poses actuales, copia directa al swapchain y composición separada de HUD/textos. No se da por arreglada la prueba temporal que el usuario vio en Quest. No se abre el visor mientras el usuario está ausente.


### Normal direct-source check / Prueba normal sin descarga de fuente

The final normal run completed three pairs from source states 1, 2 and 3 with zero failures/background rejections. It used 11,944,256 consumer bytes plus the separately bounded source pool. At 1152×648 per eye, final counters accepted 741,464 / 741,226 pixels; uncovered 2,993 / 3,231; invalid depth 0 / 0; residual rejects 2,039 / 2,039. There were 588,154 distinct eye pixels and a maximum channel span of 249. No source readback/upload or intermediate CPU pixels were enabled. Completed eye output was explicitly read back for validation. This remains a three-pair diagnostic, not continuous headset rendering or a performance measurement.

Castellano: prueba normal correcta con tres parejas de los estados 1, 2 y 3, sin incidencias ni rechazos de fondo. Consumidor: 11.944.256 bytes, además del pool de origen separado. A 1152×648 por ojo se aceptan 741.464 / 741.226 píxeles; sin cobertura 2.993 / 3.231; profundidad inválida 0 / 0; discrepancias 2.039 / 2.039. Hay 588.154 píxeles distintos entre ojos y rango de canal máximo 249. No se descargó ni subió la fuente a CPU; solo se leyó la salida terminada para comprobarla. Es un diagnóstico de tres parejas, no renderizado continuo en visor ni medición de rendimiento.


The [direct buffer presentation component](vr-direct-buffer-presentation.md) is now independently checked on GPU. Connecting it to the actual OpenXR session remains pending. Castellano: copia directa del buffer contrastada en GPU; aún falta su conexión a la sesión OpenXR real.
