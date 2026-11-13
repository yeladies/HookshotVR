# Same-state isolated lens draw / Dibujo aislado de lente desde un mismo estado

## English

The `-ShiftedDrawProbe` experiment renders four private copies of one original draw from the same submitted resources: two keep the camera unchanged, and two use opposite lens-center shifts. The game simulation and original draw run once. This is a bounded numerical/raster diagnostic, not a stereo scene, a physical eye separation or a headset mode.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -IsolatedDrawProbe -ShiftedDrawProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

The selection protocol is separate from the unchanged baseline. It requires complete read-only vertex/fragment stage inventories, resolved camera reads limited to the reviewed matrix prefix, and no primary fragment camera reads. Live geometry, targets, feedback, alias, mirror-generation and queue-completion checks remain enforced. A shader's declared used range can be smaller than its canonical NVN uniform binding; the diagnostic copies only a validated complete declared object range and preserves each descriptor's own used range. Invalid/unproven object extents reject before allocation.

For a selected depth-writing pass with no color output, the comparison uses its D32 attachment. The normal color baseline still requires color output. Two private reference draws must match the original after-image exactly. Two additional private draws start from separate pre-draw color/depth copies and receive patched camera descriptors. Their lens shifts target four pixels left/right. A visible changed-pixel mask must have comparable coverage, opposite expected horizontal centroid shifts and stable vertical centroids. Wrong direction, unchanged images, baseline mismatch, invisible output and widespread corruption do not pass. Full source-before/source-after uniform identity and both complete patched payload checks must also pass. The source descriptors are restored after all four draws; resources retire only after known completion. Buffers/images remain within the 64 MiB actual-allocation budget, excluding driver pipeline-internal allocations.

### Actual-game result

2026-11-13, Windows, RTX 2060 SUPER, ABI20, native source `ae18b93d`: first attempt passed on D32. Original changed pixels: 811. Both unchanged references: zero mismatches. Original/left/right changed-mask counts: 811/811/811. Horizontal centroids: 575.29 / 571.29 / 579.29; vertical centroids: 517.589 for all three. The complete 13568-byte camera range was validated for both private views and the source remained exact. Resources were released; peak process memory was 6471 MiB. No XR session opened.

This establishes that a real selected game shader consumes the privately patched matrices from the same submitted draw state. It does not establish physical binocular parallax, complete shaded passes, visibility coverage, per-eye histories, frame rate in Quest or full native stereo. The playable headset build remains temporal stereo. Next native work can test a translated view and expand the pass/resource closure; it must retain the unchanged controls and output checks.

## Castellano

`-ShiftedDrawProbe` dibuja cuatro copias privadas de un único dibujo original desde los mismos recursos presentados: dos sin modificar la cámara y dos con desplazamientos opuestos del centro de la lente. La simulación y el dibujo original se ejecutan una sola vez. Es un diagnóstico de matrices y rasterización; no es todavía la escena estéreo, una IPD física ni un modo para el visor.

La selección tiene un protocolo separado del baseline y exige inventarios completos de vértices/fragmentos sin escrituras laterales, lecturas de cámara resueltas dentro del prefijo revisado y ausencia de lecturas primarias de cámara en fragmentos. Se mantienen los controles de geometría, targets, feedback, alias, generaciones de mirror y finalización de cola. Se valida la extensión completa declarada del uniforme y se conserva el rango consumido de cada descriptor. No se amplía una copia a partir de un tamaño supuesto.

Los pases que escriben profundidad sin color se comprueban en D32; el baseline normal de color sigue exigiendo salida de color. Las dos referencias privadas deben coincidir exactamente con el original. Las otras dos parten de copias independientes del estado anterior al dibujo y usan cámaras parcheadas. La máscara de píxeles modificados debe ser visible, conservar una cobertura comparable y desplazarse cuatro píxeles a izquierda/derecha, sin desplazamiento vertical apreciable. Dirección incorrecta, imagen inmóvil, referencia distinta, salida invisible o corrupción no pasan. También deben coincidir los bytes originales antes/después y validarse ambos uniformes completos. Se restauran los descriptores y se libera memoria tras completar la GPU.

### Resultado real

Windows, RTX 2060 SUPER, ABI20, fuente `ae18b93d`: primer intento correcto en D32. Se modificaron 811 píxeles y las dos referencias tuvieron cero diferencias. Las tres máscaras mantuvieron 811 píxeles; sus centros horizontales fueron 575.29 / 571.29 / 579.29, y el vertical 517.589 en las tres. Se validaron los 13568 bytes completos de cada cámara y la fuente permaneció intacta. Recursos liberados y pico de proceso de 6471 MiB. No se abrió XR.

Esto demuestra que un shader real seleccionado consume las matrices privadas desde el mismo estado presentado. No demuestra todavía paralaje binocular físico, pases completos con color, visibilidad, historiales por ojo ni rendimiento en Quest. La build jugable del visor sigue usando estéreo temporal. El siguiente trabajo nativo puede probar una vista trasladada y ampliar los pases/recursos conservando estos controles.

Final validation for this checkpoint: all 92 VR tests passed. Inherited suite: 357 passed, the same 5 baseline/environment failures, 59 skipped in 298 seconds. Default-off startup reached its 20-second limit with no XR request at 3063 MiB peak. The stable installation remains separate.

Validación final: 92 pruebas VR correctas. Regresiones heredadas: 357 correctas, los mismos 5 fallos conocidos y 59 omitidas en 298 segundos. Arranque normal de 20 segundos sin solicitar XR, pico de 3063 MiB. La instalación estable permanece separada.
