# Native graphics route observations

## English

The existing incomplete frame image graph now records the actual emitted graphics command and its recording identity, colour/depth attachments, colour write masks/blending, dynamic depth comparison/test/write and viewport depth endpoints, stencil/discard state and fragment execution metadata. Mesh fallback, direct, indirect and indirect-count routes remain distinct; paths follow actual Vulkan calls rather than requested game draw flags. Compute and unknown routes are explicitly unobserved. This adds no GPU draws and does not replay simulation.

The analyser checks attachment identities against the same event's resource writes, rejects malformed state, and lists known and missing route observations within the eye-dependent presented slice. Old captures remain readable with missing coverage. A known route does not establish camera alias semantics, shader side effects, subresources, visibility, histories or pass completeness; `replay_ready` and `opaque_pass_ready` remain false.

All 192 VR checks, eight renderer syntax checks and four affected inherited resource/query checks pass. Actual-game route validation is recorded below. The separate selected-surface forward depth result is documented in [native forward depth](vr-native-forward-depth.md); it is not complete world stereo.

## Castellano

El grafo existente de imágenes del fotograma registra ahora el comando gráfico realmente emitido y su grabación, destinos de color/profundidad, máscaras y mezcla de color, estado dinámico de profundidad y su rango, stencil/descarte y metadatos del shader de fragmentos. Distingue rutas directas, indirectas, con contador y fallback de malla a partir de las llamadas Vulkan reales. Las rutas desconocidas y de cómputo quedan sin observación gráfica. No añade dibujos GPU ni repite la simulación.

El analizador comprueba que los destinos pertenecen a las escrituras del mismo evento, rechaza datos inválidos y enumera observaciones presentes y ausentes en la cadena dependiente del ojo. Las capturas antiguas se pueden leer, pero no acreditan esta cobertura. Registrar un estado no demuestra alias de cámara, efectos laterales, subrecursos, visibilidad, historiales ni un pase completo. `replay_ready` y `opaque_pass_ready` siguen siendo falsos.

Pasan 192 pruebas VR, ocho comprobaciones de sintaxis del renderer y cuatro pruebas heredadas de recursos/consultas. El resultado de este inventario con el juego real se recoge abajo. El resultado independiente de profundidad de una superficie está en [profundidad nativa](vr-native-forward-depth.md); no equivale al estéreo de todo el mundo.

## Actual-game inventory / Inventario del juego real

The matched ABI24 `3ce34e66` build completed its bounded 150-second no-XR run. In the presented dependency slice, 1196 graphics operations had valid route metadata and none lacked it. Fragment execution-mode metadata was available for those observations. The full captured frame included 1013 camera-seeded operations across 138 programs, exceeding the default 64-program shader-effect inventory. Most observed graphics commands were indexed draws; direct/indirect mesh commands were also present. This inventory does not approve these shaders for replay.

The isolated forward-depth repeat did not record a selected draw in this run because the exact camera association was unavailable. The earlier successful depth result remains independently identified by `52428c42`; it is not claimed as repeated by this inventory. Nine installation and fourteen save hashes remained unchanged, and the exact private process stopped at its wall-clock deadline. Complete native world rendering and a headset build remain pending.

La build ABI24 `3ce34e66` terminó su prueba de 150 segundos sin XR. En la cadena de dependencias presentada, 1196 operaciones gráficas tuvieron metadatos válidos y ninguna quedó sin esa observación. Se dispuso de los modos de ejecución de fragmentos de esas operaciones. El fotograma completo incluía 1013 operaciones con semilla de cámara y 138 programas, por encima del inventario de efectos de 64 programas por defecto. Predominaron dibujos indexados; también hubo comandos de malla directos e indirectos. Este inventario no autoriza a repetir esos shaders.

La repetición aislada de profundidad no llegó a grabar un dibujo seleccionado porque faltó su asociación exacta de cámara. La prueba anterior correcta conserva su identidad `52428c42`; este inventario no la da por repetida. Permanecieron idénticos nueve hashes de instalación y catorce de partidas, y el proceso privado se cerró dentro del plazo previsto. Siguen pendientes el mundo nativo completo y la build de visor.
