# Native eye camera alias closure / Alias de cámara por ojo

## English

The next native-render milestone extends the isolated draw recorder, not the depth reconstruction viewer. Before private eye allocations, it computes the transitive closure of overlapping uniform-buffer descriptors on the same resolved Vulkan buffer. It copies the bounded union once per eye and preserves each descriptor's individual range and offset relative to that copy. An overlapping storage descriptor rejects the attempt rather than retaining access to shared camera bytes. Malformed, overflowing or over-64-KiB unions are rejected before GPU allocation. Existing source snapshot, queue completion and full descriptor restoration rules remain in force.

This closes resolved byte aliases; it does **not** identify every logical game-camera allocation, hidden pointer, matrix field, physical allocation alias across different buffers, culling dependency or temporal-history input. The current preflight still requires its reviewed canonical camera seed. Camera matrices remain unchanged. No new headset-native scene is claimed.

The actual recorder tests cover offset aliases, reverse-ordered transitive overlap, nonzero copy origins, preserved payloads, storage alias rejection, overflow, the byte budget, exact source restoration and completion-gated cleanup. All 78 VR tests passed; additional alias cases passed in the recorder test. On the RTX 2060 SUPER, native source fingerprint `7836e412`, the private-save GPU baseline matched on its first attempt: 117 visibly changed original pixels, zero mismatched pixels in either private image, and all resources retired. This remains one unchanged isolated draw, not a transformed eye camera or complete stereo scene.

Next: review actual shader camera read footprints and matrix meanings; add an explicitly bounded eye-transform experiment to private UBO copies; compare original-state preservation and expected image displacement offscreen; then expand to complete producer/consumer passes before headset submission. Do not repeat game simulation per eye.

## Castellano

El nuevo avance del render nativo amplía el recorder de dibujo aislado, no el visor de reconstrucción por profundidad. Antes de reservar recursos por ojo, reúne todos los descriptores uniformes solapados que utilizan el mismo buffer Vulkan resuelto, incluidos los enlaces indirectos por solapamiento. Copia la unión acotada una vez por ojo y conserva el rango y offset relativo de cada descriptor. Rechaza alias de almacenamiento solapados y rangos inválidos, desbordados o superiores a 64 KiB antes de reservar recursos GPU. Se mantienen las comprobaciones de estado, restauración completa y finalización de cola.

Esto resuelve alias de bytes observados; **no** demuestra cobertura de toda la cámara lógica, punteros ocultos, semántica de matrices, alias físicos entre buffers diferentes, culling ni historial temporal. El preflight conserva su semilla canónica revisada. Las matrices todavía no cambian. No se presenta una nueva escena nativa en el visor.

Las pruebas del recorder real cubren offsets distintos, cadenas de solapamiento en orden inverso, orígenes no nulos, conservación de bytes, rechazo de almacenamiento compartido, desbordamiento, presupuesto, restauración y liberación tras completar la GPU. Pasaron las 78 pruebas VR y los casos adicionales del recorder. En RTX 2060 SUPER, fuente `7836e412`, el primer intento real coincidió: 117 píxeles cambiados en el dibujo original, cero diferencias en las dos copias privadas y recursos liberados. Sigue siendo un dibujo aislado sin transformar, no una escena estereoscópica completa.

Siguiente paso: revisar las lecturas reales de cámara de los shaders y sus matrices, introducir transformaciones acotadas solo en copias privadas, verificar desplazamiento y conservación del original sin visor, y ampliar después a los pases completos antes de enviar al visor. No repetir la simulación del juego por ojo.

## Headset observation / Observación del visor

On 2026-11-13 the user confirmed that the moving depth preview stayed visible and moved, with apparently correct scale and depth but a pixelated appearance. The session accepted 254 source updates in 60 seconds; this is about 4.2 capture updates/s, not a headset refresh-rate measurement. Both experimental processes closed normally. The launcher now rejects an exited producer or a packet older than two seconds before opening XR, and avoids console writes between producer and viewer startup. The earlier delayed launch showed an old frame briefly and then suppressed it. The precise reason for the launcher delay was not proven.

El 13-11-2026 el usuario confirmó que la prueba de profundidad se mantuvo visible y en movimiento, con escala y profundidad aparentemente correctas, pero pixelada. Hubo 254 actualizaciones en 60 segundos, unas 4,2 capturas/s; no es una medición de refresco del visor. Ambos procesos cerraron correctamente. El lanzador ahora rechaza productores terminados o capturas de más de dos segundos antes de abrir XR y evita escribir en consola entre ambos arranques. La ejecución anterior abrió tarde una captura antigua y la ocultó enseguida. No se ha demostrado la causa exacta del retraso.

## Regression result / Resultado de regresiones

Inherited suite: 357 passed, 5 known baseline/environment failures, 59 skipped in 374 seconds. The five failures remain test_fpx_sticky, test_builder_details_hitbox, test_builder_dlss_update, nx2/test_nup_paths and test_vram_budget_row. Default-off startup passed; all 19 monitored stable-installation hashes match. No new XR session was opened for native alias testing.

Batería heredada: 357 correctas, los 5 fallos conocidos de base/entorno y 59 omitidas en 374 segundos. El arranque con el diagnóstico desactivado pasó y coinciden los 19 hashes vigilados de la instalación estable. No se abrió otra sesión XR para esta prueba nativa.
