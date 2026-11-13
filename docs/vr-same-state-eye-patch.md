# Same-state eye patch preflight / Auditoría de cámara desde un mismo estado

## English

Temporal stereo remains the playable diagnostic. This offline audit advances the separate same-state native-render path without opening XR or changing the experimental renderer. Run the existing graphics program analyzer against a private, associated shader inventory; each program now includes `eye_patch_preflight`.

The existing complete-payload helper rewrites the reviewed view, view-projection, projection and inverse-view prefix `[0, 0xe0)`. It preserves all remaining bytes. Preservation alone does not establish that those bytes are eye-independent. The new report lists every statically observed primary-camera read inside that prefix and every read outside it, retaining shader stage, binding and descriptor-array identity. Missing modules, dynamic pointers, unsupported loads and incomplete footprints cannot pass as complete evidence. All reports retain `safe_to_patch=false` and `shader_semantics_confirmed=false`.

A private associated capture with 64 observed programs contained 16 existing read-only vertex/fragment candidates with camera-transform reads. Twelve read only within the current matrix patch extent; four also read preserved fields. This narrows a future isolated-draw experiment. It does not prove matrix meanings, all camera aliases, culling, independent storage resources, complete passes or scene stereo. Aggregate observations are documented here; shader binaries, inventories and detailed reports stay under ignored `work/`.

### Next implementation gate

1. Keep the unchanged isolated-draw comparator and baseline selection intact. A transformed experiment needs a separate explicit mode and result; a pixel mismatch must never become a successful baseline.
2. Review the exact matrix operations used by a selected program and all aliases. `matrix_footprint_only` is a transport observation, not authorization to transform.
3. Apply a tiny diagnostic transform only to each private GPU camera copy made from the same submitted bytes. Avoid replacing actual GPU bytes with an unverified CPU observation. One possible implementation is a bounded compute patch of private buffers; this still needs its own shader, synchronization and lifetime review.
4. Verify expected signed image displacement, identical nonpatched bytes, an unchanged original path, complete descriptor restoration and retirement after known completion. No simulation or shared compute/storage writes may repeat per eye.
5. Expand from one draw to complete producer/consumer passes, eye-dependent visibility and histories before submitting a native stereo world to OpenXR.

## Castellano

El estéreo temporal sigue siendo el diagnóstico jugable. Esta auditoría sin conexión al visor avanza la ruta independiente de render nativo desde un mismo estado; no cambia todavía el renderer experimental. El analizador de programas gráficos añade `eye_patch_preflight` a cada programa de un inventario privado y asociado a su captura.

La copia completa de uniformes transforma el prefijo revisado de vista, vista-proyección, proyección y vista inversa `[0, 0xe0)`, conservando el resto. Conservar bytes no demuestra que sean independientes del ojo. El informe separa las lecturas dentro del prefijo de las que necesitan revisión y conserva etapa, binding y elemento del array de descriptores. Módulos ausentes, punteros dinámicos o lecturas no resueltas impiden declarar completa la cobertura. Se mantiene siempre `safe_to_patch=false`; falta demostrar la semántica del shader.

En una captura privada asociada de 64 programas hay 16 candidatos existentes de vértices/fragmentos sin escrituras laterales y con lecturas de transformación. Doce solo leen dentro del prefijo contemplado; cuatro también leen campos conservados. Esto acota el próximo experimento de un dibujo, pero no demuestra matrices, todos los alias, culling, recursos independientes, pases completos ni estéreo de la escena. Los shaders e informes detallados permanecen privados en `work/`.

El siguiente paso requiere un modo separado del baseline sin cambios: revisar operaciones y alias, transformar mínimamente las dos copias GPU del mismo uniforme presentado y medir desplazamientos opuestos sin tocar el original. Una posible vía es un pequeño pase de cómputo sobre buffers privados, aún pendiente de implementar y verificar. Se deben conservar bytes desconocidos, restaurar descriptores y esperar a la finalización GPU para liberar recursos. Después hay que cubrir pases completos, visibilidad e historial por ojo antes de enviar una escena nativa a OpenXR. No se repite la simulación ni se duplican escrituras compartidas.

## Validation / Validación

The authored, game-free program-audit tests cover known matrix reads, preserved-field blockers, unresolved footprints, stage/array identity, malformed or out-of-range offsets and absent matrix evidence. The actual capture analysis is a separate private observation, not a CI requirement. This change does not require a native rebuild or a headset session.

Las pruebas sin archivos del juego cubren matrices conocidas, campos pendientes, lecturas no resueltas, identidad de etapa/array, offsets inválidos y ausencia de evidencia. El análisis de la captura real es una observación privada separada, no una condición de CI. Este cambio no requiere recompilar binarios nativos ni usar las Quest.
