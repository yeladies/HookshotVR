# Camera consumer capture / Captura de consumidores de cámara

## English

The first broad graphics audit on the validated 2.3.1 camera build collected 64 programs and 309 bindings, including vertex, fragment, task and mesh stages. Its 64 MiB export budget filled with 67,107,920 bytes: 17,812,476 bytes belonged to compute modules even though graphics were being inspected. Seven primary-camera modules were unavailable, so the launcher correctly kept the audit unverified. The camera/history checks and 114 first-person samples passed independently. A separate compute audit completed; its data remains private.

The optional exporter now reserves that same budget for the requested stage family. Normal shader audits retain graphics, including both NV and EXT task/mesh entry models; `-ComputeShaderUniformAudit` retains GLCompute modules. Unsupported, mixed-family, truncated and zero-length instruction streams fail closed. Filtered modules do not consume export quota. Reused module handles lose old reviewed identities before an ineligible replacement can be observed. The existing byte, program and metadata limits remain unchanged.

The game-free suite passes 182 cases, including graphics/compute isolation, a rejected graphics workload larger than the full compute quota, all task/mesh models, malformed streams and stale-handle replacement. These are metadata/transport checks with authored words, not Vulkan execution of those tiny fixtures. Matched-build real-game follow-ups are recorded below.

This capture work does not authorize shader replay or camera-tail patching. Non-prefix reads, global pointers, dynamic indexing, task/compute writes, visibility and temporal histories still need producer/consumer review before whole-scene stereo. The already validated isolated native-eye depth draw is documented separately.

## Castellano

La primera auditoría gráfica amplia sobre la cámara 2.3.1 validada recopiló 64 programas y 309 bindings, con etapas vertex, fragment, task y mesh. El cupo de 64 MiB se agotó con 67.107.920 bytes; 17.812.476 pertenecían a módulos de cómputo aunque se estaban inspeccionando gráficos. Faltaban siete módulos de cámara principal, por lo que el lanzador mantuvo correctamente la auditoría sin verificar. Pasaron por separado la cámara, el historial y 114 muestras de primera persona. Otra auditoría de cómputo terminó; sus datos permanecen privados.

El exportador opcional reserva ahora el mismo cupo para la familia solicitada. La auditoría normal conserva gráficos, incluidos task/mesh NV y EXT; `-ComputeShaderUniformAudit` conserva GLCompute. Se rechazan modelos no admitidos, familias mezcladas e instrucciones truncadas o de longitud cero. Los módulos descartados no gastan el cupo. Un handle reutilizado pierde su identidad revisada anterior antes de observar un reemplazo no admisible. No cambian los límites de bytes, programas ni metadatos.

La suite sin juego pasa 182 casos, incluidos aislamiento entre gráficos/cómputo, rechazo de gráficos que superan el cupo completo de cómputo, modelos task/mesh, datos malformados y sustitución de handles. Son pruebas de metadatos/transporte con palabras inventadas, no ejecuciones Vulkan de esos pequeños fixtures. Las comprobaciones reales con la pareja de módulos compilados se recogen a continuación.

La captura no autoriza repetir shaders ni modificar campos opacos de cámara. Lecturas fuera del prefijo, punteros globales, índices dinámicos, escrituras task/compute, visibilidad e historiales requieren revisar escritores y consumidores antes de la escena completa en estéreo. El dibujo nativo aislado de profundidad ya validado se documenta por separado.

## Scoped capture validation / Validación de captura separada

The matched `c7c1ede0` build passed all three 150-second offscreen runs: native depth control (118 first-person samples), graphics audit (120 samples, 64 programs, 356 bindings) and compute audit (124 samples, 64 programs, 79 bindings). Every observed module was available in both audits. Three graphics uniform queries remain unresolved; capture completeness does not settle their semantics. The existing budget was retained. All 182 VR checks and six renderer syntax checks pass. Inherited results remain 586 passed, 22 matching upstream failures and 40 skipped. Nine installation and fourteen save hashes match a fresh baseline taken after the user's normal game session.

La build `c7c1ede0` pasó las tres ejecuciones de 150 segundos sin visor: control nativo de profundidad (118 muestras), auditoría gráfica (120 muestras, 64 programas y 356 bindings) y de cómputo (124 muestras, 64 programas y 79 bindings). Están disponibles todos los módulos observados en ambas auditorías. Quedan tres consultas gráficas de uniformes sin resolver; disponer de las capturas no confirma su semántica. Se conserva el mismo cupo. Pasan 182 checks VR y seis comprobaciones de sintaxis. Los heredados mantienen 586 correctos, 22 fallos de upstream y 40 omisiones. Nueve hashes de instalación y catorce de partidas coinciden con un baseline nuevo tomado después de la sesión normal del usuario.
