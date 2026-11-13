# Same-state GPU eye-copy component / Copia GPU de ojos del mismo estado

## English

`vr/src/same_state_gpu_copy.h` records two GPU image copies into separate halves of a single destination image. It accepts only a complete pair with equal nonzero simulation-state identity, origin generation, pose-sample identity and the current recording value. Eye slots, format, dimensions, queue family and source/target identities must agree. The destination must have been acquired and waited; its storage must be proven nonaliasing. All validation precedes command recording. The function performs no pixel mapping, CPU upload, queue wait, allocation, submission or simulation update.

This is a recording component, not an OpenXR integration. The caller must supply genuine producer provenance, prove source/target usages and format support, single-sample single-layer 2D storage, nonaliasing memory, GENERAL source layouts and same-queue ordering. It must retain sources and destination through completion and release an acquired OpenXR image only after the copy completes. Unsupported formats, cross-queue ownership and independent producer submissions require separate integration; they are not inferred here. Two metadata stamps do not prove that complete game geometry or eye-specific visibility was rendered.

The destination is discarded, filled left/right without scaling, and transitioned to COLOR_ATTACHMENT_OPTIMAL for the intended compositor handoff. Both sources remain GENERAL. No old source image is given a new pose stamp. The function cannot generate missing views or separate a baked HUD.

Game-free guard tests verify valid eye routing, both target rectangles, barrier/layout sequence and rejection before any command for mismatched state, origin, pose, recording, eye, size, queue family, aliases and an unavailable/unproven destination. Exact RGBA8 UNORM and sRGB copies are supported; no color conversion is performed.

The explicit GPU smoke uses authored clear-color images and a private offscreen destination, not an XR swapchain:

```powershell
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --same-state-copy
```

On the RTX 2060 SUPER it verified 4096 destination pixels over two submissions, with eye colors exchanged for the second submission to detect stale reuse or eye routing errors. The source is GPU-cleared; CPU readback is used only to check the completed diagnostic result, never to feed the copy. Buffers, descriptors and original GPU bytes continue to pass the preexisting smoke checks. Destination allocations are reused after the source submission fence. If completion is uncertain, both source and target allocations are retained for process exit. No game, headset session, audio or display configuration is touched.

Next integration needs the complete eye-dependent scene producers, actual render-pose/FOV metadata, acquired/waited OpenXR images, same-queue synchronization and independent readable HUD composition. The playable headset mode remains temporal; no visual improvement or full-scene native stereo is claimed by this component.

## Castellano

El nuevo componente graba dos copias de imagen GPU en las mitades izquierda y derecha de un destino. Exige la misma identidad de estado, origen de tracking, muestra de pose y grabación actual, además de ojos, dimensiones, formato y cola coherentes. Rechaza cualquier discrepancia antes de grabar comandos. No lee píxeles en CPU, no sube capturas, no espera colas ni repite simulación.

Es un componente de grabación, todavía sin conectar a OpenXR. El productor debe demostrar procedencia, usos, formato, imágenes 2D de una muestra/capa, memoria sin alias, layouts GENERAL, orden en la misma cola y vida de recursos hasta finalizar la GPU. El destino debe estar adquirido y esperado; su liberación al runtime necesita sincronización real. Las etiquetas por sí solas no demuestran que la escena completa se haya renderizado correctamente.

La prueba real en RTX 2060 SUPER verificó 4096 píxeles del destino en dos envíos, intercambiando colores entre ojos en el segundo para detectar reutilización antigua o cruces. Los colores se generan en GPU y solo se leen en CPU para verificar el resultado final del diagnóstico. Pasan también las comprobaciones anteriores de buffers, descriptores y bytes originales. Se reutilizan asignaciones tras el fence y se retienen si la finalización es desconocida. No se abrió el juego ni el visor.

Falta enlazar los productores completos de escena por ojo, poses/FOV reales, swapchain y sincronización OpenXR, además del HUD separado. Este avance no cambia todavía la build temporal jugable ni acredita una corrección visual.
