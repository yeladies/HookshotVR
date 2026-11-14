# Coherent view parameters and missing-layer diagnostics / Vista coherente y diagnóstico de apagones

## English

The previous Combined headset run improved detail and felt smooth to the user, but Link/enemies disappeared away from the centre and multi-second blackouts remained even with engine occlusion disabled. The next **Tuned** candidate reduces source resolution to **3584×3584** and each eye to **2304×2304**. That removes 23.4% of source pixels and 19% of eye pixels relative to 4096/2560; it is not a measured FPS increase.

The guarded gameplay-view hook previously replaced its view matrix while retaining derived float eye/target/up caches and double eye/target outputs from the body view. The supported update constructs those outputs before the matrix and uses a separate gameplay camera as input. The opt-in coherence path validates the original rigid matrix, eye position, look direction, cache values and finite focus distance before updating the derived parameters from the tracked view. It preserves focus distance and leaves the controller's source camera untouched. A private receipt counts accepted and rejected updates. This establishes consistency in that view object, **not coverage of every actor/background visibility consumer**.

The perspective hook also used to change only the tangent/aspect while leaving the stored angle and sine/cosine of its half-angle at the original narrow FOV. All five scalar fields now update together at the reviewed early camera-setup site, before its aspect store and task unlock, as well as the four matrix sites. The early site holds only aspect in s0; its hook preserves that register contract. An authored fixture compiles and runs the actual hook and host writer; the prior published hook fails the new angular-cache check. This is a concrete consistency fix; full actor coverage still needs visual validation.

Tuned also sets `CullingFovDegrees=0`: projection registers and the shared tangent/aspect fields use the same raster lens. The experimental 160° split-field profile remains available as Combined. Its earlier field readback did not prove all culling paths and the fields can also serve spatial queries. Source tangent margin remains 60%, eye margin 8%; production projection metadata matches the rendered eyes.

The synthetic diagnostic can now sweep **±45° yaw or pitch**, after a centred warmup. The launcher rejects those sweeps in headset sessions. Tests exercise transformed eye/target/up values, a thousand repeated updates, corrupt-layout rejection and both angular extremes. Normal synthetic mode keeps its previous small movement.

Missing-layer diagnostics count every successfully ended XR frame, including zero-layer frames. They separate initial loading from gaps after the first submitted image, record gap durations, and distinguish stale/unavailable sources, invalid metadata, origin/tracking, runtime visibility, suppression, failures and GPU/swapchain waits. The reason an older layer is unusable and the blocker preventing a replacement are separate counters. They do not detect black pixels inside an otherwise submitted image. The 250 ms source-pose freshness gate is unchanged; old images never receive invented fresh poses.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-xr-owned-preview.ps1 -Profile Tuned
```

This opens the isolated, bounded headset run with audio and a private save snapshot. Use it only after the matching native build receipts and unattended tests pass. World rendering is still reconstructed from a single colour/depth source; native geometry stereo, complete disocclusion, motion controls and the separate HUD/menu layer remain unfinished. Source-invisible surfaces cannot be recovered by increasing resolution.

For local diagnostics, `run-game.ps1 -GpuEyeStreamProbe -SnapshotStableSaves -LoadTestSave -Seconds 150 -GpuSquareSource -GpuSquareSize 3584 -GpuEyeSize 2304 -PreviewResolution 1080p -FovMarginPercent 60 -CoherentView -TestHeadSweep WideYaw` captures bounded private output samples without opening XR. Use `WidePitch` for the other axis. Captures, guest matrices, generated code and binaries stay in ignored `work/`. The stable installation is read only.

## Castellano

La prueba Combined anterior mejoró el detalle y el usuario percibió movimiento fluido, pero Link/enemigos desaparecían fuera del centro y seguían los apagones de varios segundos incluso sin oclusión. La candidata **Tuned** reduce la fuente a **3584×3584** y cada ojo a **2304×2304**: un 23,4% y un 19% menos de píxeles que 4096/2560. Esto no es una mejora de FPS medida.

El hook sustituía la matriz de vista y conservaba las cachés derivadas de ojo/objetivo/up en float y ojo/objetivo en double correspondientes a la cámara corporal. La actualización soportada construye esos datos antes de la matriz, desde una cámara de control separada. La corrección optativa valida la matriz rígida original, posición, dirección, cachés y distancia de enfoque antes de sincronizar los datos derivados con la vista seguida. Conserva la distancia de enfoque y no modifica la cámara de origen del controlador. Un informe privado cuenta actualizaciones aceptadas y rechazadas. Esto prueba coherencia de ese objeto, **no cobertura de todos los descartes de actores/escenario**.

El hook de perspectiva también cambiaba tangente/proporción pero dejaba el ángulo y el seno/coseno de su mitad con el FOV estrecho original. Ahora se actualizan los cinco campos juntos durante la preparación de cámara, antes de escribir la proporción y liberar su tarea, además de los cuatro sitios de matriz. El sitio temprano conserva el contrato de s0, que ahí contiene solo la proporción. Una prueba inventada compila y ejecuta el hook y la escritura reales; el hook publicado anterior falla la comprobación de cachés angulares. Es una incoherencia concreta corregida; falta confirmar visualmente toda la geometría.

Tuned también usa `CullingFovDegrees=0`, con la misma lente en los registros de proyección y los campos compartidos de tangente/proporción. Combined conserva el experimento separado de 160° para comparar: su lectura de campos no demostró todos los descartes y esos campos también pueden usarse en consultas espaciales. Se mantienen el margen de tangentes del 60% en fuente y el 8% en ojos, con metadatos de proyección correspondientes.

El diagnóstico sin visor incorpora giros de **±45° horizontales y verticales**, tras comenzar centrado. El lanzador impide activar ese movimiento simulado en XR. Las pruebas comprueban ojo/objetivo/up transformados, mil repeticiones sin acumulación, rechazo de datos inválidos y ambos extremos angulares. El modo sintético normal conserva el movimiento pequeño anterior.

El diagnóstico de apagones cuenta todos los fotogramas XR finalizados correctamente, incluidos los que no llevan imagen. Separa la carga inicial de los huecos posteriores a la primera imagen y registra duración, caducidad o ausencia de fuente, metadatos, origen/seguimiento, visibilidad del runtime, supresión, fallos y esperas GPU/swapchain. Distingue el motivo por el que no sirve la imagen anterior del bloqueo de su reemplazo. No detecta píxeles negros dentro de una imagen enviada. Se conserva el límite de frescura de 250 ms y no se atribuyen poses nuevas a imágenes antiguas.

El comando anterior abre la prueba aislada y acotada, con audio y copia privada de partida, después de validar la build correspondiente. Sigue siendo reconstrucción desde una fuente color/profundidad; faltan estéreo de geometría nativa, superficies ocultas, controles de movimiento y una capa separada para HUD/menús. Aumentar resolución no recupera superficies que la fuente nunca dibujó. La instalación estable permanece sin cambios.

## Validation / Validación

Actual GPU owner fixture: **10,616,832 authored pixels**, source 3584, eyes 2304, output allocation 42,467,584 bytes, original source unchanged, no XR session and no game content. Explicit inspection fixture uploads/readbacks do not belong to the production path. Native game, wide-angle captures and headset results are recorded here as they complete; visual correction remains unconfirmed.

Game-free validation: 161/161 VR cases pass; the inherited suite remains 358 passed / 5 known environment failures / 59 skipped (336 s). Both prior view/lens hooks fail the new regression checks. The final native SDK build is source `33fe8ca1`; real-game and headset acceptance are still pending. / Validación sin juego: pasan 161/161 casos VR; la batería heredada mantiene 358 correctos, cinco fallos conocidos del entorno y 59 omitidos (336 s). Los hooks anteriores de vista/lente fallan las nuevas regresiones. La build SDK final corresponde a `33fe8ca1`; faltan comprobaciones del juego y visor.
