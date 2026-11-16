# Native-eye GPU draw / Dibujo GPU nativo por ojo

## English

**An isolated native-eye depth draw has passed inside Hookshot 2.3.1. Full-scene stereo and OpenXR presentation are still pending; there is no headset test build yet.** The public checkout retains its earlier source base. Use the reviewed [2.3.1 source overlay](vr-hookshot-2.3.1.md) and build its matching ABI 24 executable and renderer privately.

The 150-second offscreen `Lean` run of source `e4ec0ddc` recorded 119 first-person samples, loaded the private head mask and passed projection/history checks. Its recorder completed three bounded attempts. The accepted draw established:

- Two unchanged controls matched exactly: zero left/right mismatched pixels; the original depth draw changed 3,471 pixels.
- Both private eyes used native view/asymmetric-FOV prefixes from one frozen first-person packet. Their changed-pixel counts were 11,602 and 11,077, with 16,409 differences between eye outputs. These are depth-raster observations, not a full stereo image or a performance measurement.
- The complete 13,568-byte GPU source stayed unchanged. The reviewed 224-byte eye prefixes matched, and all opaque GPU tail bytes were preserved from the actual GPU source.
- Root/page aliases and indirect guards passed: four covered pages per eye, no uncovered pages or errors, and 8,340 indices for each eye. All private resources were released after GPU completion.

The source gate supports both whole and sparse VRAM mirrors. Sparse copies require every covered block to have live allocation, current binding/synchronization and no released or pending-refill overlap. The certificate covers the requested interval only; root aliasing also verifies its complete aligned pages.

The previous prototype compared a CPU view-object identity with an NVN buffer address. A separate hook now observes the primary prefix immediately after its final store, on the exact supported guest build and six checked instructions. It records the actual CPU destination with the original first-person writer tag. The renderer resolves its copied range through the current imported pool and retains the NVN address separately for root indirection. Exact source identity, source bytes, observation ambiguity and recording lifetime remain checked. Neither equal matrices nor a newer present counter can authorize another source. The observer does not write game memory and is off in ordinary gameplay.

After building the matching modules and receipt in the prepared candidate:

```powershell
./vr/run-game.ps1 -FirstPersonReadbackTest -FirstPersonNativeEyeDrawProbe -FirstPersonHistoryAudit -FirstPersonSweep Lean -SnapshotStableSaves -LoadTestSave -Seconds 150
```

This is synthetic tracking with private saves and a private head-masked overlay. It does not open an XR session. Preparation requires 16 GiB free; the existing 4 GiB in-run stop remains. The one-game-unit-per-metre scale is not physically calibrated. Peak working set in the accepted run was 6,154 MiB; this is not a comparative performance claim.

Validation: 181/181 VR checks; three actual-Vulkan authored-buffer transport cases, including deliberate tail/source corruption; five renderer syntax checks; 586 inherited checks passed, 22 failed identically to unmodified-upstream controls, and 40 were skipped. Nine stable-installation hashes and fourteen original save files remained unchanged.

Next: extend the verified source/eye binding to reviewed color and mesh/compute consumers, complete camera-field/history mapping and scene-pass resource coverage, then render the full eye-dependent scene from one state. GPU OpenXR projection layers, independent eye histories, HUD/menu composition and physical-scale calibration follow. This single primitive does not certify visibility, all game areas or whole-scene replay. The retired depth-reconstruction camera is not used.

Only authored code, tests and documentation are published. Game files, generated code, binaries, packs, captures and saves remain local. Run update/overlay tooling from the public checkout and review every newly pinned upstream version.

## Castellano

**Ha pasado un dibujo aislado de profundidad por ojo dentro de Hookshot 2.3.1. Siguen pendientes la escena completa en estéreo y su presentación a OpenXR; todavía no hay una build para probar en el visor.** La raíz pública conserva su base de código anterior. Utiliza el [parche fuente 2.3.1](vr-hookshot-2.3.1.md) y compila en privado su pareja de ejecutable/renderizador ABI 24.

La prueba `Lean` de 150 segundos sin visor, fuente `e4ec0ddc`, obtuvo 119 muestras de primera persona, cargó la máscara privada de cabeza y pasó los controles de proyección e historial. El grabador completó tres intentos acotados. El dibujo aceptado demostró:

- Los dos controles sin cambios coinciden exactamente, con cero píxeles discrepantes; el dibujo original modificó 3.471 píxeles de profundidad.
- Cada ojo usó su vista y FOV asimétrico desde un mismo paquete congelado. Cambiaron 11.602 y 11.077 píxeles, con 16.409 diferencias entre ojos. Son medidas de rasterización de profundidad, no una imagen estéreo completa ni una prueba de rendimiento.
- El origen GPU completo de 13.568 bytes quedó intacto. Coinciden los prefijos de ojo de 224 bytes y se conservan todos los datos opacos restantes desde el origen GPU real.
- Pasaron los alias de raíces/páginas y los comandos indirectos: cuatro páginas por ojo, sin huecos ni errores, y 8.340 índices para cada uno. Los recursos privados se liberaron tras completar la GPU.

La comprobación admite mirrors completos y parciales. En los parciales, todo bloque copiado debe tener memoria vigente y estar sincronizado y utilizado en la época actual, sin liberaciones ni rellenos pendientes que afecten al rango. El certificado cubre solo ese intervalo; los alias comprueban también sus páginas completas alineadas.

El prototipo anterior comparaba la identidad de un objeto de vista CPU con una dirección de buffer NVN. Un hook independiente observa ahora el prefijo justo tras su último store, protegido por la versión del juego y seis instrucciones exactas. Registra el destino CPU real y el present original del escritor de primera persona. El renderizador resuelve el rango mediante el pool actual y conserva aparte la dirección NVN para las raíces. Se mantienen las comprobaciones de identidad, bytes, ambigüedad y duración de vida. Unas matrices iguales o un contador de present más reciente no autorizan otro origen. El observador no escribe memoria del juego y está desactivado durante el juego normal.

El comando anterior se ejecuta desde el candidato preparado, después de compilar sus módulos y recibo. Usa tracking sintético, partidas privadas y la máscara privada de cabeza; no abre OpenXR. Exige 16 GiB libres al preparar y conserva la parada a 4 GiB durante la prueba. La escala de una unidad por metro no está calibrada físicamente. El pico de memoria fue de 6.154 MiB, sin establecer una mejora comparativa de rendimiento.

Validación: 181/181 checks VR; tres casos Vulkan reales con buffers inventados, incluida corrupción deliberada de cola/origen; cinco comprobaciones de sintaxis del renderizador; 586 checks heredados correctos, 22 fallos idénticos a controles de upstream sin modificar y 40 omisiones. Siguen intactos nueve hashes de la instalación estable y catorce archivos de partida originales.

Siguiente paso: extender el vínculo comprobado a consumidores de color y mesh/compute revisados, completar el mapa de campos/historiales y cubrir los recursos de los pases de escena. Después vendrán el dibujo completo desde un estado común, las capas GPU de OpenXR, los historiales por ojo, HUD/menús y la calibración de escala. Esta primitiva no certifica toda la escena, la visibilidad ni todas las zonas del juego. No se utiliza la cámara retirada de reconstrucción de profundidad.

Solo se publican código propio, pruebas y documentación. Archivos del juego, código generado, binarios, packs, capturas y partidas permanecen locales. Ejecuta las herramientas de actualización desde la raíz pública y revisa cada nueva versión upstream fijada.
