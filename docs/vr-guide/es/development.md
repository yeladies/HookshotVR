# Arquitectura, compilación y actualizaciones

[Inicio](index.md) · [Cómo empezar](howto.md) · [Estado](status.md) · [Desarrollo](development.md) · [Ayuda](help.md) · [English / Castellano](../en/development.md)

## Mapa del código

| Ruta | Función |
|---|---|
| `vr/src/` | Herramientas OpenXR, visor, matemáticas de poses y validación de paquetes |
| `vr/shaders/` | Shaders originales del visor |
| `gpu/vk/vk_xr*`, `gpu/vk/xr_*` | Integración opcional del renderer y diagnósticos acotados |
| `vr/tests/` | Contratos, formatos y pruebas de actualización sin juego |
| `vr/upstream.json` | Base revisada, versión del juego, ABI y archivos de integración |
| `docs/vr-*.md` | Notas técnicas y registros de validación |
| `work/` | Datos y compilaciones privados; nunca publicar |

El productor captura color, profundidad y matrices de cámara del mismo fotograma. Publica un paquete acotado mediante sustitución atómica. El visor lo valida, reutiliza un buffer GPU y reconstruye la superficie visible para poses y campos de visión independientes por ojo. Rechaza datos inválidos u obsoletos. Este visor es independiente de la investigación del render nativo completo.

Consulta [transporte en movimiento](../../vr-live-depth-preview.md), [reconstrucción congelada](../../vr-frozen-depth-preview.md), [cámaras](../../vr-camera-integration.md) y [registro técnico](../../vr-development-checkpoint.md), disponibles como referencias técnicas en inglés. Los cambios en las tablas públicas host/renderer exigen subir ABI; los headers de shaders se regeneran desde sus fuentes.

## Actualizar Hookshot

El mod es una adaptación de código, no una DLL universal. La base registrada es `940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`. Aplicar el parche sin conflictos no garantiza compatibilidad con nuevas versiones del proyecto ni del juego.

Con el repositorio limpio y los cambios confirmados, añade upstream una sola vez si falta, descarga referencias y elige la revisión:

```powershell
git remote add upstream https://github.com/NotRexed/Hookshot.git
git fetch upstream
python -I -B vr/update_upstream.py audit --revision upstream/main
python -I -B vr/update_upstream.py prepare --revision upstream/main
```

Utiliza un commit o etiqueta explícitos en vez de `upstream/main` para fijar una evaluación. `prepare` crea un worktree aislado en `work/upstream-updates/`, conserva conflictos y deja `compatibility=unverified` y `release_ready=false`. No instala nada. Revisa la integración, recompila en privado con traducción nueva cuando proceda, ejecuta pruebas, verifica juego plano y después visor antes de promover el perfil. Conserva un commit funcional para volver atrás.

## Comprobaciones

```powershell
ctest --test-dir work/vr-build --output-on-failure
python -I -B vr/tests/test_update_upstream.py
python -I -B tests/run_all.py --jobs 2
```

La batería heredada necesita el compilador y dependencias de sus fixtures. Registra los fallos reales. Las pruebas sintéticas GPU son comprobaciones locales explícitas, no requisitos portables de CI. Las pruebas de visor requieren una persona que pueda observar el resultado.

Consulta [las reglas completas de actualización](../../vr-upstream-updates.md). Estas guías bilingües forman parte del parche permitido y acompañan a los candidatos de actualización.
