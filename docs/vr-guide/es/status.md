# Estado, resultados y hoja de ruta

## Estado actual: Hookshot 2.3.1 / ABI 24

El experimento nativo ha pasado un **dibujo aislado de profundidad del juego real** desde un mismo estado congelado, con cámara propia para cada ojo. La fuente `e4ec0ddc` obtuvo 119 muestras de primera persona; el intento aceptado conservó el origen GPU, hizo coincidir exactamente los dos controles, verificó uniformes y alias/comandos indirectos, y liberó sus recursos privados. Las pruebas usan seguimiento sintético.

**Todavía no es estéreo de la escena completa ni una build para el visor.** Falta revisar e integrar color, mesh/task, cómputo, recursos completos e historiales por ojo antes de presentar a OpenXR. La cámara anterior por reconstrucción de profundidad está retirada. [Pruebas, instrucciones y límites](../../vr-native-eye-draw.md) · [Parche reproducible para 2.3.1](../../vr-hookshot-2.3.1.md).

Siguientes pasos:

1. Relacionar los demás campos e historiales de cámara con sus escritores y shaders reales, incluido el culling mesh/task.
2. Cubrir pases completos de color/profundidad con recursos por ojo, actualizando simulación y trabajo compartido una vez.
3. Enviar ambos ojos completos a OpenXR directamente en GPU y componer HUD, textos y menús legibles.
4. Probar en Quest escala, visibilidad, transiciones, antigüedad de frames y estabilidad; añadir mecánicas de movimiento tras estabilizar el render.

Pasan 182/182 checks VR. Los heredados registran 586 correctos, 22 fallos conocidos de upstream y 40 omisiones. Siguen intactos nueve hashes de la instalación y catorce archivos de partida originales. Solo se comparten fuente, pruebas y documentación; todos los derivados del juego quedan locales.

La exportación separa ahora gráficos y cómputo dentro del cupo existente: están disponibles todos los módulos observados y quedan tres consultas gráficas sin resolver. El historial con identidad exacta de escritor pasó 32/32 muestras en una prueba real WideYaw. La captura Lean quedó incompleta y los intentos de color nativo permanecieron invisibles; ambas limitaciones siguen abiertas. [Pruebas y límites de captura](../../vr-camera-consumer-capture.md).

## Experimentos archivados del renderizador

Lo que sigue conserva prototipos anteriores y sus solicitudes históricas de prueba; no es el plan actual de ejecución.

**Primera persona:** el juego local ya dispone de anclaje a raíz/ojos, cachés coherentes, un paquete de cámara inmutable y máscara privada de geometría de cabeza. Sigue pendiente el estéreo nativo de la escena completa. [Implementación y pruebas aisladas](../../vr-first-person.md).

**Rama de reconstrucción de cámara — 15-11-2026.** La cámara por profundidad y sus lanzadores están retirados; el prototipo queda archivado. Existe un módulo C nuevo con rig/poses separados y proyección asimétrica, todavía sin renderizado completo por ojo. [Estado actual, diagnóstico y pasos pendientes](../../vr-camera-rebuild.md). Las entradas siguientes documentan pruebas históricas.

Prueba Quest del 15-11-2026: el usuario acepta geometría/FOV de Tuned, fluidez buena y detalle decente; queda un halo menor alrededor de Link. Los apagones persisten, aunque reducidos. Los nuevos contadores relacionan las capas omitidas con poses fuente antiguas, sin fallos de seguimiento/API. Se prueba Performance 3200/2304 con el mismo binario y se conserva Tuned 3584. [Resultados e interpretación](../../vr-coherent-view-and-gaps.md).

La build Tuned `33fe8ca1` ya está compilada: pasan ambos giros de ±45° con vista/lente coherentes, 660/780 pares GPU, seis muestras cambiantes y cero fallos del productor. Pasa el arranque sin VR y siguen intactos 19 hashes de la instalación estable. El siguiente paso requiere valoración visual en Quest/VDXR, incluidos los nuevos contadores de apagones. No se da por aceptada en visor.

La siguiente [candidata Tuned y diagnóstico de apagones](../../vr-coherent-view-and-gaps.md) usa fuente 3584 / ojos 2304 con autorización del usuario, sincroniza datos derivados de vista, recupera campos compartidos de proyección coherentes y añade giros automáticos de ±45°. La corrección visual sigue pendiente.

El usuario confirma mucho más detalle y movimiento percibido fluido con fuente 4096 y ojos 2560. Desactivar la oclusión solo mejoró algo: siguen desapareciendo Link/enemigos fuera del centro y continúan los apagones. [Resultados y propuesta de diagnóstico de cámaras/visibilidad y fotogramas sin capa](../../vr-combined-visibility-quality.md). Se conserva la calidad; la aceptación visual global sigue pendiente.

La primera sesión VDXR por GPU mostró el mundo 3D (2.654 pares / 6.363 proyecciones y cierre correcto). El usuario señaló pixelado, bordes al girar y contornos oscuros. La comparación con fuente HD, margen en los ojos y filtrado no produjo mejora sustancial según el usuario. Ningún perfil está aceptado visualmente. [Siguiente etapa e instrumentación](../../vr-presentation-age-and-next-renderer.md).

Alfa OpenXR GPU del mismo estado: [prueba y limitaciones](../../vr-gpu-world-preview.md).

## Ojos GPU continuos — 2026-11-14

La ejecución aislada corregida completa/publica 2.190 parejas GPU de fuente común a 1024×1024 por ojo, con cámara sintética, cero incidencias y sin transporte del preview antiguo. Tres muestras de salida cambian con el tiempo. La coherencia independiente de resolución elimina la rejilla negra en ojos cuadrados; oráculo GPU de 49.152 píxeles. Pasan 114 pruebas VR; regresiones 358 correctas / 5 conocidas / 59 omitidas. Faltan consumo por la sesión/swapchain real y HUD; aún no es una build validada en visor. Consulta [flujo continuo, instrucciones y límites](../../vr-continuous-gpu-eyes.md).

## Pareja nativa desde GPU viva — 2026-11-14

La prueba aislada ya reconstruye los dos ojos desde la misma fuente GPU viva, sin descargarla/subirla: tres parejas, 1152×648 por ojo, 588.154 píxeles distintos y ninguna incidencia registrada. Solo se lee la salida para inspección. Pasan 110 pruebas VR. La matemática de poses recientes está contrastada por separado; faltan presentación OpenXR, HUD y prueba en visor. Consulta la [ruta nativa, instrucciones y límites](../../vr-live-gpu-depth-pair.md).

## Origen GPU actual — 2026-11-14

El pool nativo ha completado/publicado 300 paquetes GPU inmutables de color/profundidad, sin incidencias registradas y con 17.915.904 bytes reservados en el juego aislado. Pasan 108 pruebas VR. Se descargó un solo paquete para inspección. Aún falta conectar el origen vivo al reconstructor de ambos ojos y a OpenXR. El usuario confirmó audio correcto en la prueba jugable posterior; siguen pendientes parpadeo, bordes y HUD. Consulta el [diseño y cómo probar el origen aislado](../../vr-gpu-scene-source.md). Las observaciones fechadas anteriores son históricas.

[Inicio](index.md) · [Cómo empezar](howto.md) · [Estado](status.md) · [Desarrollo](development.md) · [Ayuda](help.md) · [English / Castellano](../en/status.md)

Estado registrado el **13-11-2026**, implementación de referencia `69f7b622f518d0618226f5ffc99464706f9bbecc`. Equipo: Windows, RTX 2060 SUPER, 16 GB RAM. Conexión Quest 3: Virtual Desktop / VDXR.

## Resultados verificados

- La imagen de cine se vio correctamente en Quest. Esto no valida los nuevos modos de profundidad.
- Movimiento: 67 pares estereoscópicos actualizados en 15 segundos sin visor, aproximadamente **4,5 actualizaciones de captura/s**. No son FPS del visor ni latencia de movimiento a imagen.
- Dos imágenes de diagnóstico de 800×600: media de 1458 microsegundos para la medición del par renderizado y 7550 para lectura/archivo de verificación. Son mediciones de diagnóstico separadas.
- La captura utilizó una lectura reutilizable de 5971968 bytes; memoria máxima del proceso del juego de unos 6612 MiB.
- Una prueba GPU sintética mantuvo fijo el color y cambió profundidad: 45 fotogramas aceptados, 44 cambios de profundidad y movimiento visible. No necesita juego.
- 78 pruebas VR superadas. La batería heredada registró 357 correctas, 5 fallos conocidos de base/entorno y 59 omitidas. No está completamente en verde; consulta [el registro](../../vr-development-checkpoint.md).
- Los hashes de 19 archivos de la instalación estable no cambiaron en la última validación.

No se afirma compatibilidad de ejecución con Linux, AMD, todo el juego ni percepción o rendimiento de los nuevos modos en Quest. La escala es provisional. El sonido en Quest sigue sin verificarse/resolverse.

## Hoja de ruta incremental

1. **Prueba humana pendiente:** comprobar los modos congelado y en movimiento en Quest; registrar escala, orientación, estereoscopía, seguimiento y estabilidad.
2. **Mejorar el diagnóstico:** cadencia, memoria y latencia, manteniendo recursos y duración acotados. La reconstrucción desde una sola vista seguirá teniendo límites de oclusión.
3. **Estereoscopía nativa:** completar recursos por ojo, alias de cámara, culling, dependencias de cómputo e historial temporal. Una actualización de simulación debe alimentar ambos ojos; no repetir la lógica por ojo.
4. **Cámara nativa 6DoF:** poses, escala, recentrado, transiciones y visibilidad del cuerpo en primera persona.
5. **Controles y mecánicas:** locomoción, comodidad, manos, apuntado, armas e interacciones físicas; preservar progreso y colisiones.
6. **Preparación de lanzamiento:** regresiones de juego y visor, presupuestos de rendimiento, perfiles compatibles y candidatos de actualización revisados.

Es un orden de trabajo, no un calendario. [oot-vr](https://github.com/gibranfelix/oot-vr) es una referencia para investigar futuras mecánicas, no una dependencia ni prueba de que su implementación pueda trasladarse directamente. Revisar licencias antes de reutilizar código.

## Actualización de la prueba en Quest

El usuario confirmó después una escena de profundidad persistente y en movimiento, escala y profundidad aparentemente correctas y pixelación. Se aceptaron 254 actualizaciones en 60 segundos y la prueba cerró correctamente. La estereoscopía nativa sigue pendiente. Consulta [el avance de cámara y sus límites](../../vr-native-camera-aliases.md).

## Inspección remota de ojos — 2026-11-13

Un ensayo real sin visor ya guarda muestras privadas y acotadas de ambos ojos. La lente fuente es de 110° horizontal / 100° vertical con seguimiento sintético e imágenes temporales distintas. Se capturó una imagen del inventario, pero falta corregir su entrega continua. La geometría de los bordes requiere revisar el culling. Esto no valida profundidad percibida, comodidad, audio ni rendimiento en Quest. No se abre XR mientras el usuario está en remoto. Consulta el [experimento actual y las instrucciones de captura](../../vr-temporal-stereo-preview.md).

Tras la primera comparación de FOV/estéreo, el [trabajo de calidad y continuidad](../../vr-preview-quality.md) añade mono HD optativo, margen de lente y conservación acotada de pares. Falta comprobarlo en Quest. El estéreo HD no pasó frescura y no se da por listo para el visor.

## 16-11-2026: profundidad nativa por ojo

Una prueba privada del mismo estado confirmó que reutilizar la profundidad de la cámara central con `EQUAL` rechazaba el color de los ojos nativos. La nueva prueba dibuja la superficie opaca seleccionada con color y profundidad D32 propios por ojo. Pasó las comprobaciones de un dibujo del juego real y 190 pruebas VR. Aún no es una escena completa para el visor: quedan la acumulación de pases y la cobertura de recursos por ojo. Consulta los [detalles](../../vr-native-forward-depth.md).

El observador nativo ha pasado una ejecución real de 150 segundos: 1196 operaciones gráficas dependientes del ojo en la cadena presentada tienen estado observado válido, sin registros de ruta ausentes. Falta cubrir shaders y pases completos; la repetición aislada de profundidad no se grabó en esta ejecución. Consulta la [cobertura de rutas](../../vr-native-graphics-route.md). También está corregida la prueba de caché detectada en la batería completa; pasan las seis comprobaciones relacionadas.

### Avance del renderer nativo, 17 de noviembre de 2026

El inventario privado de shaders admite hasta 256 programas conservando los presupuestos de bytes. El fotograma medido cubrió sus 129 programas con semilla de cámara; es evidencia de cobertura, no permiso para repetirlos todos. Un transporte independiente de vértices/fragmentos amplía el experimento aislado de color/D32 por ojo a dibujos directos indexados y no indexados. Las pruebas horizontal y vertical siguen siendo inconcluyentes: el dibujo de color original seleccionado no cambió píxeles de referencia, aunque los ojos privados produjeron color y profundidad visibles.

Una consulta opcional de cobertura original distingue resultados no disponibles, cobertura cero y cobertura positiva sin cambiar la comparación de imágenes ni aceptar por ello un resultado estéreo. Está desactivada por defecto, evita las consultas de oclusión del juego y solo lee después de completar GPU. La consulta real devolvió cobertura cero disponible; la selección posterior encontró candidatos positivos sin completar el dibujo clásico. Siguen pendientes la pasada del mundo nativo y la build de visor; el experimento de superficies aisladas no es un lanzador VR jugable. Consulta el [transporte clásico y sus límites](../../vr-native-classic-transport.md).

La fuente privada más reciente `10b88bd6` pasa 199 pruebas VR, ocho comprobaciones de sintaxis y 586 pruebas heredadas, con los mismos 22 fallos de base y 40 omitidas. Se observan cobertura positiva y avance de la selección; faltan el dibujo clásico completo por ojo, la acumulación del mundo nativo y la build de visor.
