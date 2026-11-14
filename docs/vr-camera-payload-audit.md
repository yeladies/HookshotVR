# Completed-GPU camera payload audit

## English

The isolated root probe can now export the **complete original camera payload** after GPU completion, full eye-matrix/root/indirect verification, exact unchanged image controls and a byte-for-byte before/after source comparison. It exports no live guest memory. This supports review of the camera fields used by global shaders; it does not change those fields or enable scene replay.

Each run receives a fresh private `work/game-run/camera-payload-<nonce>.json` path. Only the opt-in `-GpuRootAliasProbe` enables it. The source remains mapped until serialization finishes. Every 32-bit word is written as an unsigned integer, preserving pointers, nonfinite bit patterns and other non-float data. Canonical payloads are bounded to 560–65536 bytes and the output to 256 KiB. I/O errors leave the render diagnostic unverified as an export, without changing source buffers or raising an exception through the rendering path.

The record includes recording, pool/capture generation, draw/program scope, guest range and dimensions. The offline reader validates field types, extents and guest windows, then independently checks the finite matrix prefix, view/projection product and inverse view. Unknown nonfinite words outside the prefix remain valid preserved data. Metadata records native observations; an arbitrary JSON file's origin is not independently authenticated.

`vr/audit_camera_payload.py` accepts up to eight samples, finds matching matrix/position values and intersects offsets across samples. It reports distinct views and distinct reference values separately. Repeating a stationary camera does not establish multi-view evidence. Output candidates are capped, source words stay unchanged, and `safe_to_patch`, `field_semantics_confirmed` and `shader_consumption_verified` remain false.

### First actual-game sample

The complete 13,568-byte source exported and passed offline coherence checks; the root/depth diagnostic also passed. One preserved word outside the prefix was nonfinite and retained its exact bits. Peak process working set: 6623 MiB. This sample contains one distinct view, so the following are algebraic candidates only:

| Candidate | Matching byte offsets |
|---|---|
| View 3×4 | 0, 336, 2576 |
| View-projection 4×4 | 48, 384 |
| Projection 4×4 | 112, 448, 2672 |
| Inverse view 3×4 | 176, 512, 2624 |
| Inverse projection 4×4 | 2736 |

No contiguous world-camera XYZ position or inverse view-projection matrix matched. Repeated matrices can be current caches, previous-frame state or coincident data; their semantic roles are not inferred. Additional samples with a changing camera are necessary before considering more fields for private-eye updates.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -IsolatedDrawProbe -GpuRootAliasProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
python -I -B vr/audit_camera_payload.py <private-camera-payload.json> --output work/validation/camera-payload-analysis.json
```

`-ProbeCameraTurn Left` or `Right` adds an opt-in game-local right-stick movement before publishing the native selection. It requires the root diagnostic, a private test-save snapshot and at least 120 seconds. Duration is bounded to 75–2000 ms. It never drives the desktop or a headset. The resulting matrices, rather than the fact that input was sent, determine whether a distinct view was obtained.

The source payloads, reports and game captures remain under ignored private work paths. Do not upload them. Only the authored serializer, analyzer, launcher controls, tests and aggregate documentation belong in the repository. Full-scene stereo, camera-field semantics, private histories and separate HUD/OpenXR composition remain pending.

## Castellano

El diagnóstico de raíces ya exporta la cámara original completa después de verificar finalización GPU, matrices de ambos ojos, alias, argumentos indirectos, referencias de imagen exactas y fuente idéntica antes/después. No lee memoria invitada viva ni modifica campos. Cada ejecución usa un archivo privado nuevo; solo se habilita con `-GpuRootAliasProbe`.

Se conservan todos los bits mediante enteros de 32 bits, incluidos punteros y palabras que no son floats válidos. El bloque está limitado a 560–65536 bytes y el JSON a 256 KiB. Los errores de escritura se informan sin cambiar las fuentes ni lanzar excepciones por la ruta de renderizado. El analizador comprueba tipos, tamaños, rangos invitados y coherencia de las matrices del prefijo. Los valores no finitos fuera del prefijo se mantienen intactos.

La primera muestra real tiene 13.568 bytes y una vista distinta; pasó la coherencia independiente y el diagnóstico de profundidad/raíces. Conserva una palabra no finita fuera del prefijo. Pico de proceso 6623 MiB. La tabla anterior recoge coincidencias algebraicas: hay más copias de vista/proyección fuera de la región que modificamos actualmente, pero podrían ser cachés, historial o coincidencias. No se ha encontrado una posición XYZ contigua ni una inversa completa de vista-proyección que coincidan.

La herramienta admite hasta ocho muestras y diferencia número de vistas de variación de cada referencia. Una cámara repetida no cuenta como confirmación adicional. Las coincidencias no autorizan modificar campos: semántica, consumo por shaders y permiso de parche siguen sin confirmar.

El giro opcional `-ProbeCameraTurn Left/Right` se limita al stick derecho del juego aislado. Exige el diagnóstico de raíces, copia privada del guardado y al menos 120 segundos; dura entre 75 y 2000 ms. El archivo de selección se publica después de iniciar el movimiento. No opera el escritorio ni el visor. Los datos obtenidos deben demostrar que cambió la vista.

Los archivos del juego y de captura permanecen locales. Solo se publica código propio, pruebas y documentación agregada. Continúan pendientes escena completa, significado de los campos, historiales privados y composición separada de HUD/OpenXR.


### Three-view follow-up

All three game runs passed the guarded depth/root/camera checks and exported complete payloads. The two game-local stick tests produced genuinely different views. Across three distinct views, matching current-view candidates remain at 0 and 2576 bytes, inverse-view candidates at 176 and 2624. The matrix at 336, VP at 384 and inverse view at 512 that matched in stationary captures no longer match during the moving-camera sample. This rules out treating every stationary match as the same current-camera field. It does not independently identify those other fields as history.

Projection candidates stay at 112, 448 and 2672, and inverse projection at 2736. Projection varied zero times across these runs; these repeated values are not multi-lens confirmation. View, VP, inverse view and derived camera position each have three distinct reference values. No contiguous position or inverse VP match was found. The tool still authorizes no field updates.

Castellano: las tres ejecuciones pasaron el diagnóstico completo y generaron cámaras con tres vistas distintas. Coinciden vista en 0/2576 e inversa de vista en 176/2624. Las coincidencias anteriores de 336, 384 y 512 dejan de coincidir durante el movimiento; no se deben modificar todas las copias iguales observadas en reposo. Esto tampoco demuestra por sí solo que las otras sean historial. La proyección no cambió entre estas muestras, por lo que sus copias siguen siendo candidatos sin confirmación con lentes distintas. No se autoriza modificar campos.

Validation: all 105 VR tests pass; native inherited regression 358 passed, the same five known baseline/environment failures, 59 skipped (339 seconds). Native Windows compilation succeeds. No headset session or human test was opened.

Validación: 105 pruebas VR correctas; regresión nativa 358 correctas / 5 fallos conocidos / 59 omitidas en 339 segundos. Compilación Windows correcta, sin abrir sesión ni solicitar prueba del visor.

Final controls: the separate default-off 20-second startup passed at 4783 MiB with XR and camera export disabled. All 19 monitored stable-install hashes remain unchanged. Windows documentation encoding was corrected to explicit UTF-8; no raw camera values or game artifacts are published.

Controles finales: arranque separado de 20 segundos correcto con XR y exportación desactivados. Los 19 hashes estables siguen intactos. Documentación corregida a UTF-8 explícito, sin publicar valores de cámara ni archivos del juego.
