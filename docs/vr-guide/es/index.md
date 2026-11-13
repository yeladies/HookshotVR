# HookshotVR — guía del proyecto

[Inicio](index.md) · [Cómo empezar](howto.md) · [Estado](status.md) · [Desarrollo](development.md) · [Ayuda](help.md) · [English / Castellano](../en/index.md)

Investigación experimental de PCVR para **Hookshot**, orientada a Quest 3 en Windows mediante Virtual Desktop / VDXR. El objetivo es render estereoscópico nativo, seguimiento de cabeza con seis grados de libertad y, después, interacción con mandos. **Todavía no es un mod VR nativo terminado ni jugable.**

## Empieza aquí

- [Compilar y probar](howto.md): requisitos, compilaciones aisladas y pruebas acotadas.
- [Estado y hoja de ruta](status.md): resultados, mediciones y trabajo pendiente.
- [Arquitectura y actualizaciones](development.md): código, validación y adaptación a Hookshot.
- [Problemas y colaboración](help.md): informes útiles y reglas de publicación.

## ¿Qué hay disponible?

| Modo | Evidencia actual |
|---|---|
| Pantalla plana de cine | Imagen y pantalla fija en el espacio confirmadas en Quest; sonido sin resolver. |
| Reconstrucción de profundidad congelada | Proyecciones y poses independientes por ojo implementadas; prueba GPU sin visor superada; percepción en visor pendiente. |
| Reconstrucción de profundidad en movimiento | Escena en movimiento confirmada en Quest; escala/profundidad aparentemente correctas; pixelación observada. Render nativo pendiente. |
| [Cámara del juego vivo](../../vr-immersive-game-preview.md) | Nuevo prototipo inmersivo monoscópico; la cámara real recibe poses de cabeza; prueba de visor pendiente. |
| Render nativo de toda la escena por ojo | En investigación; se ha reproducido un dibujo aislado sin modificar, no una escena estereoscópica completa. |

Los modos de profundidad reconstruyen las superficies visibles desde una cámara del juego. No recuperan geometría oculta ni equivalen a render nativo. La cámara de origen sigue en tercera persona. Los controles VR, las armas físicas y el audio están pendientes.

Aquí solo se distribuyen código, shaders propios, pruebas y documentación. No hay compilación pública del juego ni descarga VR lista para jugar. Necesitas tus propios datos locales de una versión compatible.

[Rama de desarrollo](https://github.com/yeladies/HookshotVR/tree/vr/bootstrap) · [Hookshot original](https://github.com/NotRexed/Hookshot) · [Registro técnico en inglés](../../vr-development-checkpoint.md)
