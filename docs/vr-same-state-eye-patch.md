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

## GPU transport component / Componente de transporte GPU

The authored `eye_matrix_patch.comp` diagnostic now copies the actual source GPU payload into two independent buffers, validates its rigid-view/perspective prefix, applies opposite small view translations and recomputes view-projection and inverse-view fields. Unknown fields and the projection remain exact. Invalid matrices or an excessive translation leave the copied payload unchanged. This is an uncalibrated diagnostic displacement, not a physical IPD or a gameplay eye camera.

The explicit native GPU smoke uses entirely authored data and opens neither a game window nor XR:

```powershell
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --matrix-patch
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --matrix-patch-reject-shift
work/vr-build/hookshot_vr_eye_resource_smoke.exe --run --matrix-patch-reject-view
```

On the RTX 2060 SUPER all three modes passed. Two submissions reuse allocations after fence completion. The positive mode compares the two GPU results to an independent CPU matrix helper, checks opposite projected-point displacement, verifies original bytes and all preserved fields, and reads each private payload through its physical address and coherent UBO/SSBO routes. Numerical tolerance applies only to recomputed matrix values; view rows, projection and unknown fields have exact-byte checks. Both negative modes require an unchanged copy. The original GPU smoke also passed. Clear-color images remain diagnostic images; no game geometry is transformed by these tests.

An additional native transport mode can exercise a selected real-game baseline draw:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -IsolatedDrawProbe -GpuEyeMatrixProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

It keeps the strict unchanged-draw comparison and its restored descriptors. The two newly patched buffers are **not bound to a game draw**. Four full uniform snapshots (source before/after and private left/right) are compared only after GPU completion. Source mutation, changed view rows, changed projection, altered unknown bytes, incoherent matrices or a rounded-away displacement reject the transport proof. Original draw provenance, allocation budgets, aliases, no-parallel-replay rules and completion-gated cleanup remain enforced. This mode must run separately from XR; the runner clears it by default.

Castellano: el shader propio copia el uniforme GPU real a dos buffers independientes, valida vista rígida y perspectiva, aplica traslaciones pequeñas opuestas y recalcula vista-proyección y vista inversa. La proyección y los campos desconocidos conservan sus bytes. Una entrada inválida deja la copia sin transformar. No representa una IPD calibrada.

Los tres comandos del smoke pasaron en la RTX 2060 SUPER, además de la prueba GPU anterior. Se comprueban el oráculo CPU, desplazamientos proyectados opuestos, bytes originales, campos conservados, lecturas por dirección física/UBO/SSBO y reutilización tras completar la GPU. Las imágenes son colores de diagnóstico, no geometría del juego.

El nuevo modo nativo toma cuatro snapshots completos de uniformes alrededor de un dibujo real: fuente antes/después y dos copias transformadas. **Estas copias todavía no se enlazan al dibujo.** El baseline sigue exigiendo imágenes idénticas y restauración completa. Solo se valida el transporte después de completar la GPU; cualquier modificación del original o de campos conservados se rechaza. El lanzador lo desactiva por defecto y exige una prueba aislada sin XR. El siguiente paso sigue siendo revisar la semántica del programa y probar un dibujo transformado en un modo separado.

Actual-game transport result (ABI20 `e0b6e4f9`): first attempt passed with a complete 2432-byte camera range, unchanged source bytes and both transformed private payloads validated. The two unchanged baseline images had zero mismatched pixels and 112 changed original pixels; resources retired normally. Peak process memory 6628 MiB. Transformed buffers were not bound to a draw. All 91 VR tests passed; inherited results remain 357 passed / 5 known failures / 59 skipped.

Resultado real (ABI20 `e0b6e4f9`): primer intento correcto con 2432 bytes completos, fuente intacta y dos copias privadas transformadas validadas. Los dibujos de referencia coincidieron exactamente y se liberaron los recursos. Pico de proceso 6628 MiB. Las copias transformadas no se enlazaron al dibujo. Pasan 91 pruebas VR; regresiones 357 correctas / 5 fallos conocidos / 59 omitidas.

The separate [isolated lens-shift draw](vr-same-state-isolated-draw.md) now exercises a different explicit shader mode: it changes projection center and recomputes VP, while keeping the view fixed. Its verification uses actual D32 draw masks and two unchanged references. It must not be confused with calibrated eye translation or the unbound default view-transport mode.

El [dibujo aislado con lente desplazada](vr-same-state-isolated-draw.md) usa un modo explícito distinto: cambia el centro de proyección y recalcula VP, manteniendo la vista. Se verifica sobre máscaras D32 reales y dos referencias intactas; no equivale a traslación física calibrada ni al modo de transporte sin enlazar las copias.
