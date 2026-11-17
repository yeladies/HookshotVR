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

## Expanded audit capacity / Capacidad de auditoría ampliada

The captured frame uses 138 camera-seeded programs, so the default inventory of 64 programs cannot establish complete shader coverage. An explicit private audit may set `HOOKSHOT_VR_SHADER_UNIFORM_MAX_PROGRAMS` to `128` or `256`. Absent, malformed or excessive values retain 64. The existing 64 MiB module-export and 4 MiB metadata budgets remain unchanged, as do module identity, scope and replay admission checks. The option expands observations only; it does not authorize additional draws. Four tests exercise the exact default/128/256 bounds and an excessive request; all 196 VR checks pass. The later `86871e96` private run captured 256 programs within the byte budgets and covered all 129 camera-seeded programs in its measured frame; it did not authorize complete-world replay.

El fotograma capturado usa 138 programas con semilla de cámara; el inventario de 64 no basta para revisar todos. Una auditoría privada explícita puede usar `HOOKSHOT_VR_SHADER_UNIFORM_MAX_PROGRAMS` con `128` o `256`. Un valor ausente, inválido o excesivo conserva 64. Siguen vigentes los límites de 64 MiB de módulos y 4 MiB de metadatos, las identidades, el ámbito y las reglas de admisión. La opción amplía observaciones; no autoriza dibujos adicionales. Cuatro pruebas cubren los límites exactos y una petición excesiva; pasan las 196 comprobaciones VR. La prueba privada posterior `86871e96` capturó 256 programas dentro de los presupuestos y cubrió los 129 programas con semilla de cámara de su fotograma; no autorizó la repetición del mundo completo.

The launcher uses `-ShaderAuditProgramLimit 128` or `256` together with shader auditing. Offline analyzers require the matching explicit `--max-programs`; their default remains 64 and inputs above the selected capacity are rejected. Both count and byte bounds apply.

El lanzador usa `-ShaderAuditProgramLimit 128` o `256` junto con la auditoría. Los analizadores requieren el `--max-programs` explícito correspondiente; conservan 64 por defecto y rechazan entradas que excedan el valor seleccionado. Se aplican tanto el límite de programas como el de bytes.
