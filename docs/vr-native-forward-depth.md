# Native per-eye forward depth probe

## English

This is an offscreen, single-draw experiment, not a complete stereo scene or a headset release.

The same-state private comparison on 2026-11-16 confirmed a depth mismatch: native eyes produced zero visible pixels with the original camera's depth and `EQUAL`, but the identical eye matrices, geometry, roots and materials produced 5098/5077 visible pixels with comparison `ALWAYS`. Unchanged reference images matched exactly and source resources were preserved. `ALWAYS` is diagnostic only; it cannot supply correct occlusion.

The new opt-in `RX_VK_XR_NATIVE_FORWARD_DEPTH_PROBE=1` retains that comparison and adds separate native colour and D32 depth snapshots. Each eye clears its own private depth to the far endpoint and draws the selected opaque surface with depth writes enabled and `LESS_OR_EQUAL` or `GREATER_OR_EQUAL`. Ordering is derived from the validated perspective projection and actual viewport depth range. Projection coefficients, including the near/far terms, remain unchanged.

Admission requires the original opaque prepass state (depth test enabled, `EQUAL`, writes disabled, blending disabled), reviewed side effects and a fragment module with known execution modes, no early tests and no depth replacement. Metadata is derived on compilation and cache loading; the disk cache format and public ABI 24 do not change. Unsupported paths remain rejected. Source depth state, root and descriptors are restored after private commands.

Only this explicit diagnostic permits a 96 MiB recorder allocation budget for twelve readback images and private GPU images; the ordinary 64 MiB limit is unchanged. Root-alias allocations retain their separate existing bound. Source/control equality, visible native colour, actual non-clear D32 values, colour/depth overlap, eye differences and completion-gated cleanup must all be measured. A colour-only result is rejected. Authored tests cover reversed projection and viewport ordering, overlapping surfaces, incompatible shader modes and restoration. The actual-game single-draw result is recorded below.

Full-world rendering still requires accumulation across all relevant draws and passes, per-eye visibility, resource ownership, transparency, histories and OpenXR delivery. One selected draw over a copied centre-camera image does not meet that requirement.

## Castellano

Es una prueba de un solo dibujo sin visor; no representa una escena estereoscópica completa ni una versión lista para jugar.

La comparación privada del mismo estado confirmó el 16-11-2026 una discrepancia de profundidad: los ojos nativos no dibujaron píxeles visibles con la profundidad original y `EQUAL`; con las mismas matrices, geometría, raíces y materiales, usando `ALWAYS`, aparecieron 5098/5077 píxeles. Los controles coincidieron exactamente y los recursos originales se conservaron. Desactivar ese rechazo sirve para diagnosticar, pero no resuelve la oclusión.

La nueva opción `RX_VK_XR_NATIVE_FORWARD_DEPTH_PROBE=1` añade capturas independientes de color y profundidad D32. Cada ojo limpia su profundidad privada al extremo lejano y dibuja la superficie opaca seleccionada con escritura de profundidad y comparación `LESS_OR_EQUAL` o `GREATER_OR_EQUAL`. El orden se deriva de la proyección validada y del rango real del viewport. Se conservan los coeficientes de proyección y los planos cercano y lejano.

Solo se admite el estado opaco de prepass original, sin mezcla, junto con efectos revisados y un shader de fragmentos compatible, sin pruebas tempranas ni sustitución de profundidad. Los metadatos se reconstruyen al compilar y cargar la caché; no cambia su formato ni la ABI pública 24. Después se restauran el estado de profundidad, la raíz y los descriptores originales.

Esta prueba explícita tiene un límite de 96 MiB para el recorder y sus doce imágenes de lectura; el límite habitual de 64 MiB y el presupuesto separado de alias se mantienen. La validación exige controles intactos, color visible, profundidad D32 escrita, coincidencia espacial entre ambos, diferencias entre ojos y liberación tras terminar la GPU. Color sin profundidad no pasa. Las pruebas propias cubren profundidad invertida, superficies superpuestas, shaders incompatibles y restauración. El resultado de esta nueva ruta con un dibujo real se recoge abajo.

Para una escena completa quedan la acumulación de dibujos y pases, visibilidad por ojo, propiedad de recursos, transparencias, historiales y envío a OpenXR. Un objeto sobre una copia de la cámara central no cumple ese objetivo.

## Actual-game result / Resultado con el juego real

The matched ABI24 build `52428c42` completed the isolated forward-depth test on 2026-11-16. The successful selected draw produced 3273/3243 visible colour pixels with non-clear D32 depth at every such pixel, and 5939 differing eye pixels. The two unchanged controls had zero mismatches; source matrices and preserved tails matched; private resources retired normally. The test used `LESS_OR_EQUAL`, depth clear 1 and 64,251,904 recorder bytes within its 96 MiB bound. Nine installation hashes and fourteen save hashes were unchanged. The exact private process stopped at the 150-second wall-clock deadline.

This is a single-draw result. Root routing and private-copy coverage were checked; complete root read semantics and full-world stereo remain unverified. The general launcher still reports its broader gameplay/history requirements as unmet; this isolated result does not override them.

La build ABI24 `52428c42` completó la prueba aislada el 16-11-2026. El dibujo seleccionado produjo 3273/3243 píxeles de color con profundidad D32 escrita en todos ellos, y 5939 píxeles distintos entre ojos. Los dos controles no tuvieron diferencias; matrices, campos conservados y liberación de recursos fueron correctos. Se usaron `LESS_OR_EQUAL`, limpieza de profundidad a 1 y 64.251.904 bytes del recorder dentro de 96 MiB. Permanecieron idénticos nueve hashes de instalación y catorce de partidas. El proceso privado se cerró al cumplir 150 segundos.

El resultado corresponde a un dibujo. No demuestra toda la semántica de las lecturas por raíz ni el estéreo del mundo completo. El lanzador general sigue indicando que faltan sus requisitos más amplios de juego e historiales; esta prueba aislada no los sustituye.
