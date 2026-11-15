# Hookshot 2.3.1 source overlay / Parche fuente para Hookshot 2.3.1

## English

**Experimental source, compatibility unverified. No headset test build is ready.** The overlay reproduces the reviewed first-person adaptation on the exact 2.3.1 source. Its 179 VR contracts pass; native game validation on this updated base is still pending. The earlier depth/temporal camera is retired. Complete native scene rendering for both eyes and XR presentation remain pending.

The source in the root checkout retains its earlier recorded base and ABI 21. Use the overlay candidate for 2.3.1; it uses **ABI 24**, extending upstream ABI 23. Both the executable and renderer must be built from the same candidate. The supported guest build remains `e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348`.

From the root of this branch, fetch the pinned official source, then prepare a separate working directory:

```powershell
git fetch https://git.notrexed.dev/Hookshot/Hookshot.git 5d3de413df5587a1896b2b7df6a3a385ae24195a
. ./vr/compatibility.ps1
$python = Get-VrPython ''
$result = & $python -I -B vr/prepare_overlay.py v2.3.1 | ConvertFrom-Json
Set-Location -LiteralPath $result.candidate_directory
```

The helper verifies the patch hash, actual changed paths, resulting Git tree and ABI. It never fetches, builds, launches, installs or publishes. Its report remains `compatibility=unverified` and `release_ready=false`. The original checkout and installed game are preserved. Review the [maintenance rules](vr-upstream-updates.md) and upstream Windows setup before building privately. Keep the Builder's hot-function lists, chunk layout and profiles so the development build retains the normal optimizations. Use a fresh CMake directory if an update removed the old compiler paths.

The overlay contains only reviewed code, authored shaders, tests and documentation. The official source now includes scrubbed saves; publishing its entire tree/history would import those files. This repository distributes the VR patch instead. The prepared upstream-based working directory, saves, game files, generated code, captures and binaries remain local. A future release needs another pinned review and validation, rather than assuming an arbitrary new DLL is compatible.

## Castellano

**Código experimental, compatibilidad sin verificar. Todavía no hay una build preparada para probar en el visor.** El parche reproduce la adaptación de primera persona sobre el código exacto de 2.3.1. Pasan sus 179 contratos VR; falta validarla dentro del juego actualizado. La cámara anterior por profundidad/estéreo temporal está retirada. Siguen pendientes el dibujo nativo completo de ambos ojos y la presentación XR.

El código de la raíz conserva su base anterior y ABI 21. Para 2.3.1 utiliza el candidato creado por este parche, que usa **ABI 24** sobre el ABI 23 oficial. Hay que compilar ejecutable y renderizador desde el mismo candidato. El juego admitido sigue siendo `e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348`.

Desde la raíz de esta rama, ejecuta los comandos anteriores: descargan la revisión oficial fijada y preparan otra carpeta de trabajo. El helper comprueba el hash del parche, las rutas reales de los cambios, el árbol Git resultante y el ABI. No descarga, compila, ejecuta, instala ni publica nada. Su informe mantiene `compatibility=unverified` y `release_ready=false`. Conserva el checkout inicial y la instalación del juego.

Antes de compilar en privado, revisa [las reglas de mantenimiento](vr-upstream-updates.md) y la preparación de Windows del upstream. Incluye las listas de funciones calientes, la distribución de fragmentos y los perfiles del Builder para mantener las optimizaciones de la versión normal. Usa otra carpeta CMake si la actualización eliminó las rutas de los compiladores anteriores.

Solo se distribuyen código revisado, shaders propios, pruebas y documentación. El upstream incluye ahora partidas depuradas: publicar su árbol/historial completo también las incorporaría. Por eso este repositorio distribuye el parche VR. El directorio preparado, las partidas, archivos del juego, código generado, capturas y binarios permanecen locales. Cada versión futura requerirá fijar otra revisión y validarla; una DLL nueva cualquiera no garantiza compatibilidad.
