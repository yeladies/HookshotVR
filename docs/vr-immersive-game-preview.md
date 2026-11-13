# Live game head-camera prototype / Prototipo de cámara del juego

## English

This is an **experimental monoscopic immersive projection** of the running game. The guarded main-game camera hook consumes fresh tracked head poses and changes the real game view matrix before its normal rendering. It does not reconstruct geometry from a depth packet. Both compositor eyes receive the same source view and capture pose: there is no native eye separation yet. The camera remains third person, with the original camera FOV, limited peripheral coverage, possible culling/transition issues and no head collision or physical VR weapon controls. Audio is off by default. This is a new headset test, not a finished native-stereo mod.

### Run the private prepared test

Connect Quest 3 through Virtual Desktop, close other Hookshot instances, then run the development PC's private `work/headset-immersive-preview/Probar-juego-inmersivo.cmd`. Put the headset on so the runtime can produce focused tracking poses. Loading the private save can take about 90 seconds. The whole run lasts 180 seconds including preparation and closes automatically. Until a valid gameplay camera is associated, no immersive image is submitted. A restricted source FOV can leave black peripheral regions.

Source command:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -Seconds 180
```

This requires matching privately compiled ABI20 host/renderer receipts and the reviewed game build in `vr/upstream.json`; the launcher is not a standalone downloadable game. Keep all binaries/data private. Optional `-Window` shows a windowed monitor copy; `-Audio` requests audio but routing is not verified. The first test uses neither. Standard gamepad input is a separate check; tracked controller actions are not integrated.

Inspect small yaw/pitch/roll rotations, then a few centimetres of translation. Check whether new geometry appears when looking around rather than stretching the old image. Describe scale, FOV, pixelation, image latency and stability. Do not walk around: there is no head collision, and the camera experiment rejects movement beyond one metre from its tracking anchor. Remove the headset if uncomfortable. Reference-space generation changes reset the anchor.

### Implementation and validation

The opt-in `head_camera` callback originated in ABI19. Current sources use **ABI20**, with present identity and a tracked-lens callback. The hook retains supported-build/vtable/main-camera guards and excludes the debug free camera. A game-independent helper validates rigid matrices, finite unit quaternions, pose age, translation bounds and tracking generation; it avoids accumulating transforms when an unchanged view is revisited.

Submitted main-camera uniforms must match a recent transformed view in a bounded history. Projection FOV comes from that source block. Metadata is attached to the exact renderer present before delayed readback publication. Missing metadata, changed origin or stale images suppress the layer. The compositor receives two projection views with the same source head pose/FOV/image, explicitly mono. GPU queue locking and upload-fence ownership reuse the tested transport. No game simulation is repeated per eye.

Offscreen reproduction, with synthetic movement and **no XR session**:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -ImmersiveReadbackTest -SnapshotStableSaves -LoadTestSave -Seconds 150
```

Synthetic/XR combinations are rejected before game setup; the native callback also rejects synthetic motion while XR is active. Game-free checks cover pose math, nonaccumulation, invalid/stale input, source FOV and reanchor; a fake runtime validates the projection layer and suppression of unassociated images. The first real GPU/isolated-save run recorded 1206 camera overrides, 1200 tagged presents and 600 associated images, using 6677 MiB peak working set. These counts are not headset FPS, latency or a human visual validation. Final-build results are recorded in the development checkpoint.

## Castellano

Es una **proyección inmersiva monoscópica experimental del juego en ejecución**. El hook protegido de la cámara principal recibe poses recientes de la cabeza y modifica la matriz de vista real antes del render habitual. No reconstruye geometría desde un paquete de profundidad. Ambos ojos reciben la misma vista y pose de captura: todavía no hay separación estereoscópica nativa. La cámara sigue en tercera persona, conserva el FOV original y puede mostrar periferia negra y problemas de culling/transiciones. No tiene colisión de cabeza ni armas/mandos VR físicos. El audio está desactivado por defecto.

### Probar el paquete privado

Conecta las Quest mediante Virtual Desktop, cierra otras instancias de Hookshot y ejecuta `work/headset-immersive-preview/Probar-juego-inmersivo.cmd` en el PC de desarrollo. Ponte el visor para que el runtime facilite poses con foco. La partida privada puede tardar unos 90 segundos en cargar. La duración total es de 180 segundos, incluida la preparación, y se cierra automáticamente. No se envía imagen inmersiva hasta asociar una cámara de juego válida.

Comando fuente:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -Seconds 180
```

Necesita los binarios privados y recibos ABI20 correspondientes y el juego revisado en `vr/upstream.json`; no es una descarga independiente. `-Window` muestra una copia en ventana en el monitor y `-Audio` solicita sonido, cuya ruta no está verificada. La primera prueba no utiliza ninguna de esas opciones. El mando convencional requiere su propia comprobación; las acciones de mandos VR no están integradas.

Prueba giros pequeños en los tres ejes y después desplazamientos de pocos centímetros. Comprueba si aparecen superficies nuevas al mirar alrededor en lugar de estirar la imagen anterior. Describe escala, FOV, pixelación, retraso y estabilidad. No camines por la habitación: no hay colisión de cabeza y el experimento rechaza desplazamientos superiores a un metro desde el anclaje. Quita el visor si aparece malestar. Un cambio de origen de seguimiento reinicia el anclaje.

### Qué se ha comprobado

El callback de cámara se introdujo en ABI19; las fuentes actuales utilizan **ABI20**, con identidad de present y lente seguida. Se mantienen los filtros de versión, vtable, cámara principal y exclusión de cámara libre. Las pruebas validan matrices, cuaterniones, antigüedad de pose, límites y recentrado, además de evitar acumulación de transformaciones. La imagen solo se presenta con metadatos asociados al present correcto y al origen de seguimiento vigente. Los dos ojos utilizan la misma imagen y pose: sigue siendo mono. No se repite la simulación por ojo.

La prueba sin visor utiliza `-ImmersiveReadbackTest` y movimientos sintéticos; se impide combinarlos con XR. El runtime simulado verifica la capa de proyección y el rechazo de imágenes sin asociación. El primer ensayo real registró 1206 cambios de cámara, 1200 presents asociados y 600 imágenes con pose; pico de memoria de 6677 MiB. No son FPS del visor ni una validación visual humana. Consulta el registro técnico para los resultados de la compilación final.

Todos los archivos del juego, binarios, capturas, partidas e informes privados permanecen bajo `work/` y nunca se publican.

## Final build / Compilación final

Source `8e6576af`, ABI19: 82/82 VR tests passed; actual game/offscreen test verified 725 camera overrides and 360 associated images, with 6550 MiB peak working set. Inherited regressions: 357 passed, 5 known baseline/environment failures, 59 skipped. Default-off startup and 19 unchanged stable hashes checked. The new mode has not yet been visually checked in Quest.

Fuente `8e6576af`, ABI19: 82/82 pruebas VR correctas; ensayo real sin visor con 725 cambios de cámara y 360 imágenes asociadas, pico de 6550 MiB. Regresiones heredadas: 357 correctas, 5 fallos conocidos y 59 omitidas. Arranque ordinario y 19 hashes estables comprobados. La validación visual de este nuevo modo en Quest está pendiente.

## New optics experiment / Nueva prueba de óptica

Current sources use ABI20. See [wide FOV and temporal stereo](vr-temporal-stereo-preview.md) for two distinct eye views and the wider source lens. Mono headset feedback confirmed smooth head response and original controls, with a window-like field, black menus and muted audio. New stereo headset checks remain pending.

Las fuentes actuales utilizan ABI20. Consulta [FOV amplio y estéreo temporal](vr-temporal-stereo-preview.md). La prueba mono confirmó seguimiento fluido y controles originales, con efecto de ventana, menús negros y audio desactivado. La nueva prueba de visor está pendiente.
