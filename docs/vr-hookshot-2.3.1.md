# Hookshot 2.3.1 source overlay / Parche fuente para Hookshot 2.3.1

## English

**Experimental source, compatibility unverified. No headset test build is ready.** The overlay reproduces the reviewed first-person adaptation on the exact 2.3.1 source. Its 181 VR contracts pass, alongside three authored-buffer Vulkan GPU cases. The reference first-person camera and eye-uniform transport have passed an offscreen real-game run; an isolated native-eye depth draw has also passed. Full-scene validation is still pending. The earlier depth/temporal camera is retired. Complete native scene rendering for both eyes and XR presentation remain pending.

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

**Código experimental, compatibilidad sin verificar. Todavía no hay una build preparada para probar en el visor.** El parche reproduce la adaptación de primera persona sobre el código exacto de 2.3.1. Pasan sus 181 contratos VR y tres casos Vulkan de buffers sintéticos propios. La cámara de referencia y el transporte de uniformes han pasado una prueba real sin visor; también ha pasado un dibujo nativo aislado de profundidad por ojo. Falta validar la escena completa. La cámara anterior por profundidad/estéreo temporal está retirada. Siguen pendientes el dibujo nativo completo de ambos ojos y la presentación XR.

El código de la raíz conserva su base anterior y ABI 21. Para 2.3.1 utiliza el candidato creado por este parche, que usa **ABI 24** sobre el ABI 23 oficial. Hay que compilar ejecutable y renderizador desde el mismo candidato. El juego admitido sigue siendo `e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348`.

Desde la raíz de esta rama, ejecuta los comandos anteriores: descargan la revisión oficial fijada y preparan otra carpeta de trabajo. El helper comprueba el hash del parche, las rutas reales de los cambios, el árbol Git resultante y el ABI. No descarga, compila, ejecuta, instala ni publica nada. Su informe mantiene `compatibility=unverified` y `release_ready=false`. Conserva el checkout inicial y la instalación del juego.

Antes de compilar en privado, revisa [las reglas de mantenimiento](vr-upstream-updates.md) y la preparación de Windows del upstream. Incluye las listas de funciones calientes, la distribución de fragmentos y los perfiles del Builder para mantener las optimizaciones de la versión normal. Usa otra carpeta CMake si la actualización eliminó las rutas de los compiladores anteriores.

Solo se distribuyen código revisado, shaders propios, pruebas y documentación. El upstream incluye ahora partidas depuradas: publicar su árbol/historial completo también las incorporaría. Por eso este repositorio distribuye el parche VR. El directorio preparado, las partidas, archivos del juego, código generado, capturas y binarios permanecen locales. Cada versión futura requerirá fijar otra revisión y validarla; una DLL nueva cualquiera no garantiza compatibilidad.

## Native-eye experiment / Experimento nativo por ojo

[English and Spanish design, validation and limitations](vr-native-eye-draw.md). The reference 2.3.1 first-person port compiled locally as a matched ABI 24 host/renderer (source `43c6fe79`). This updated overlay adds an isolated native-eye draw experiment; earlier versions compiled and ran, exposing sparse-buffer and CPU-object/NVN-address identity mismatches. The guarded writer revision passed its isolated depth-raster validation as source `e4ec0ddc`, with intact controls, source preservation and GPU alias/readback checks. The standalone GPU transport checks use authored synthetic inputs and make no graphics draws. Neither result certifies native scene stereo or a headset build. The inherited suite reports 586 passes, 22 failures matching unmodified-upstream controls, and 40 platform/tool skips.

[Diseño, validación y límites en inglés y castellano](vr-native-eye-draw.md). El port de referencia de primera persona para 2.3.1 ha compilado localmente como pareja ejecutable/renderizador ABI 24 (fuente `43c6fe79`). El parche actualizado añade un experimento aislado de dibujo nativo por ojo; las versiones anteriores compilaron y se ejecutaron, detectando problemas con buffers parciales y con las identidades CPU/NVN. La revisión con observador del escritor pasó la rasterización aislada de profundidad como fuente `e4ec0ddc`, con controles intactos, origen conservado y alias/lecturas GPU verificados. Las pruebas independientes de transporte GPU usan datos sintéticos propios y no hacen dibujos gráficos. Estos resultados no certifican estéreo nativo de la escena ni una build para el visor. La suite heredada registra 586 comprobaciones correctas, 22 fallos que coinciden con controles de upstream sin modificar y 40 omisiones por plataforma/herramientas.

## Reference-port validation / Validación del port de referencia

The earlier ABI 24 port (source `43c6fe79`) passed a 150-second offscreen real-game Lean test: 39 first-person samples, private head mask loaded, native-depth projections audited, and two 13,568-byte eye uniform uploads verified and retired on the GPU. The test used synthetic tracking, not a headset. Nine installation hashes and fourteen original save files remained unchanged. This validates the reference camera port; this result does not certify the separate native-eye primitive drawing path. First-person diagnostic preparation now requires 16 GiB free and retains its 4 GiB in-run stop threshold.

El port ABI 24 anterior (fuente `43c6fe79`) ha pasado una prueba real del juego de 150 segundos sin visor, con barrido Lean sintético: 39 muestras de primera persona, máscara privada de cabeza cargada, proyecciones con profundidad nativa auditadas y dos uniformes de ojo de 13.568 bytes verificados y retirados en GPU. Siguen intactos nueve hashes de la instalación y catorce archivos de partida originales. Esto valida el port de referencia de la cámara; este resultado no certifica la ruta independiente de dibujo de primitivas por ojo. La preparación del diagnóstico exige ahora 16 GiB libres y mantiene la parada durante ejecución a 4 GiB.
