# Estado, resultados y hoja de ruta

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
