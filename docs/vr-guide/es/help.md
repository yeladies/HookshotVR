# Problemas, informes y colaboración

[Inicio](index.md) · [Cómo empezar](howto.md) · [Estado](status.md) · [Desarrollo](development.md) · [Ayuda](help.md) · [English / Castellano](../en/help.md)

## Problemas frecuentes

| Síntoma | Qué comprobar |
|---|---|
| No detecta visor / error OpenXR | Conexión de Virtual Desktop y runtime VDXR configurado. Ver el escritorio remoto no demuestra que OpenXR esté disponible. |
| Solo veo una pantalla de cine | Has iniciado la prueba plana. Sigue la guía de profundidad para superficies reconstruidas; el render nativo sigue en desarrollo. |
| No hay sonido | La integración y ruta de audio no están verificadas. Una prueba silenciosa no significa que el audio esté terminado. |
| Huecos o superficies ausentes | Una vista de profundidad no contiene geometría oculta; empieza con traslaciones pequeñas de cabeza. |
| Movimiento a saltos | La última cadencia de captura sin visor fue de unas 4,5/s, independiente del bucle del visor. No se han medido FPS en Quest para este modo. |
| Recibo obsoleto/incompatible | Recompila los componentes aislados correspondientes. No copies DLL aleatorias a la instalación estable. |
| Desaparece la escena | Comprueba cierre/timeout del productor y frescura del paquete; los datos obsoletos se ocultan deliberadamente. |
| Monitor sin señal | Detén la prueba y recupera la conexión antes de repetir. Anota el síntoma exacto; no es lo mismo que un pantallazo azul de Windows. |

## Informe útil

Anota commit, Windows/GPU/driver, runtime, visor, comando, duración y resultado. Identifica el modo. Para profundidad describe ambos ojos, escala, color, orientación, pequeños desplazamientos de cabeza, cadencia, cierre y estabilidad del monitor. No adjuntes paquetes, geometría, binarios, partidas ni configuración privada del juego. Revisa los logs de texto para quitar rutas personales e identificadores antes de publicarlos.

Utiliza [las incidencias del proyecto](https://github.com/yeladies/HookshotVR/issues) para problemas del mod. Reproduce los problemas propios de Hookshot sin este mod antes de comunicarlos al proyecto original. Este proyecto acepta informes en inglés y castellano.

## Publicación y colaboración

Envía cambios concretos, pruebas y resultados honestos. Lee [las reglas de contribución](../../../CONTRIBUTING.md) y [las instrucciones del repositorio](../../../AGENTS.md). Se conservan el código heredado y sus licencias. Es una adaptación experimental independiente, no una versión VR oficial del proyecto original.

Publica código propio del mod, shaders propios, pruebas y documentación. **Nunca subas archivos del juego, claves, código generado del juego, shaders extraídos, capturas, mallas reconstruidas, partidas, configuración privada, `work/` ni paquetes locales de ejecutables/DLL.** Subir todo significa todo el trabajo publicable, no los datos privados del juego.

Todavía no hay versión lista para jugar, garantía de futuras actualizaciones, 6DoF nativo completo ni armas físicas implementadas. No hace falta donar ni comprar hardware para seguir el proyecto.
