# GPU camera aliases and private guest roots

## English

The same-state stereo work now copies and patches the GPU guest-page table for each eye. This closes a camera route that replacing uniform-buffer descriptors alone cannot cover: another shader can reach the same camera through a guest address. This is an isolated diagnostic, not complete scene rendering or an updated headset preview.

The root has 3,145,728 entries (24 MiB per eye), covering the backend's two guest-address windows with 4096-byte pages. A compute pass copies the entire submitted root, finds every full physical-page overlap with the camera's source pages, and redirects it to that eye's private copy. All other entries stay exact. The scan discovers aliases absent from a CPU inventory. Partial overlaps, unaligned entries, overflow and uncovered camera reads reject verification rather than inventing a mapping. The original table and camera remain unchanged.

A second pass verifies every destination entry. A third reads the whole camera back through the private root, only dereferencing pages inside the owned private camera allocation. CPU retirement compares every returned byte against the separately validated GPU matrix payload. Source metadata must match the actual recording, device, pool and capture generation. The metadata query never initializes or imports a table. Allocation is capped at 96 MiB for this component; together with the parent diagnostic's cap, the maximum is 160 MiB. No completion wait is added.

A separate authored GPU guard produces indirect arguments with zero vertices/indices unless the alias counters and every camera byte agree. It also checks bounded geometry and instance counts. Positive, partial-overlap, misaligned-entry and deliberately corrupted-payload tests pass on the RTX 2060 SUPER, with two submissions and allocation reuse. This guard has not yet been connected to a game draw that reads global memory.

Game-free tests compile the actual root recorder with an in-memory Vulkan double. They cover each setup-allocation failure, an allocation exception, stale pool generations, camera payloads crossing page boundaries, low/high-window aliases, wrong alias offsets, in-flight retention, completed retirement, and permanent retention after device ownership becomes uncertain. The double tests lifecycle and transport; actual shader execution is tested separately on the GPU.

### Local game evidence

The first native run passed on its first selected depth draw. Both eyes found four camera aliases, with zero uncovered overlaps or root/lookup errors. All 13,568 camera bytes matched the validated eye payloads. Private allocations were 50,391,584 bytes and retired after queue completion. The depth comparison retained two exact unchanged controls and opposite horizontal camera displacement; the original camera stayed byte-exact. No XR session was opened.

The private root is currently consumed by the authored lookup shader only. Existing private depth draws still use their reviewed uniform descriptors. Global-memory game shaders, complete colored scene production, HUD extraction/composition and OpenXR delivery remain pending. Physical eye separation and world scale are not calibrated. The playable temporal preview is unchanged.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -IsolatedDrawProbe -GpuRootAliasProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

This developer command uses ignored private runtime/save copies and cannot combine with XR or the color experiment. Run only one game diagnostic at a time. Do not upload the generated runtime, reports, game shaders, captures, saves or binaries. The authored compute shader, its generated header, tests and documentation are source material.

## Castellano

La ruta hacia estÃƒÆ’Ã‚Â©reo desde el mismo estado ya copia y modifica la tabla GPU de pÃƒÆ’Ã‚Â¡ginas invitadas para cada ojo. AsÃƒÆ’Ã‚Â­ se cubre un acceso a cÃƒÆ’Ã‚Â¡mara que cambiar descriptores de uniformes no resuelve: otro shader puede llegar a los mismos datos por una direcciÃƒÆ’Ã‚Â³n invitada. Sigue siendo un diagnÃƒÆ’Ã‚Â³stico aislado, no la escena completa ni una nueva prueba del visor.

Se recorren los 3.145.728 elementos de la tabla, 24 MiB por ojo, con pÃƒÆ’Ã‚Â¡ginas de 4096 bytes. Se redirigen todos los alias completos hacia las pÃƒÆ’Ã‚Â¡ginas privadas del ojo; el resto queda intacto. Solapamientos parciales, direcciones desalineadas, desbordamientos o lecturas sin cobertura rechazan la verificaciÃƒÆ’Ã‚Â³n. Otra pasada verifica la tabla completa y otra lee la cÃƒÆ’Ã‚Â¡mara exclusivamente desde pÃƒÆ’Ã‚Â¡ginas privadas conocidas. Al finalizar la GPU se comparan todos los bytes con las matrices de cada ojo. Se comprueban dispositivo y generaciones de grabaciÃƒÆ’Ã‚Â³n, captura y memoria. No se introduce una espera de GPU.

El componente tiene un lÃƒÆ’Ã‚Â­mite de 96 MiB; sumado al diagnÃƒÆ’Ã‚Â³stico padre, el mÃƒÆ’Ã‚Â¡ximo es 160 MiB. Las pruebas del cÃƒÆ’Ã‚Â³digo real cubren fallos de reserva, cambios de generaciÃƒÆ’Ã‚Â³n, cÃƒÆ’Ã‚Â¡maras que cruzan pÃƒÆ’Ã‚Â¡ginas, alias de ambas ventanas de direcciones y liberaciÃƒÆ’Ã‚Â³n solo despuÃƒÆ’Ã‚Â©s de terminar. Si cambia el dispositivo y la propiedad deja de ser fiable, los recursos no se reutilizan ni se liberan mediante un identificador reciclado.

Un guardia GPU separado genera un dibujo indirecto de cero elementos si fallan los contadores o difiere cualquier byte de cÃƒÆ’Ã‚Â¡mara. Han pasado en la RTX 2060 SUPER los casos positivo, solapamiento parcial, direcciÃƒÆ’Ã‚Â³n desalineada y datos alterados, con dos envÃƒÆ’Ã‚Â­os y reutilizaciÃƒÆ’Ã‚Â³n de reservas. TodavÃƒÆ’Ã‚Â­a no se conecta a un dibujo real que lea memoria global.

La primera prueba nativa del juego pasÃƒÆ’Ã‚Â³ al primer dibujo de profundidad seleccionado: cuatro alias por ojo, cero accesos sin cubrir y cero errores. Coincidieron los 13.568 bytes de cada cÃƒÆ’Ã‚Â¡mara. Se reservaron 50.391.584 bytes y se liberaron tras finalizar la cola. Las referencias del dibujo coincidieron y las vistas trasladadas mostraron desplazamientos opuestos. Fuente intacta y XR desactivado.

La tabla privada todavÃƒÆ’Ã‚Â­a la consume solo nuestro shader de comprobaciÃƒÆ’Ã‚Â³n. Faltan shaders del juego con memoria global, escena completa con color, HUD separado y entrega a OpenXR. TambiÃƒÆ’Ã‚Â©n falta calibrar escala e IPD. El preview temporal jugable no cambia. Los archivos generados del juego permanecen locales; solo se publica cÃƒÆ’Ã‚Â³digo propio, pruebas y documentaciÃƒÆ’Ã‚Â³n.


### 2026-11-14 validation follow-up

The rebuilt native module passed a second actual-game run on the first attempt: changed masks 818/818/818, horizontal centroids 575.413 / 573.537 / 577.295, vertical centroids 517.557 / 517.564 / 517.577; unchanged controls had zero mismatches. Both private roots again found four aliases, zero uncovered entries and zero errors, with all 13,568 payload bytes validated and allocations retired. Peak working set was 6420 MiB. All 102 VR tests pass. A separate bounded 20-second default-off startup passed at 4786 MiB without XR. All 19 monitored stable-install hashes match. These process memory observations are separate runs, not a measured performance improvement.

Castellano: segunda prueba nativa con el mÃƒÆ’Ã‚Â³dulo recompilado, correcta al primer intento: mÃƒÆ’Ã‚Â¡scaras 818/818/818 y referencias sin diferencias. Cuatro alias por ojo, ningÃƒÆ’Ã‚Âºn acceso sin cobertura ni error, 13.568 bytes completos verificados y recursos liberados. Pico de proceso 6420 MiB. Pasan las 102 pruebas VR. Arranque de 20 segundos con opciones nuevas desactivadas correcto, 4786 MiB y sin XR. Coinciden los 19 hashes estables. Las cifras de memoria pertenecen a ejecuciones distintas y no demuestran una mejora de rendimiento.

Inherited native regression: 358 passed, the same five documented baseline/environment failures, 59 skipped, 351 seconds. No new inherited failure was observed.

RegresiÃƒÂ³n nativa heredada: 358 correctas, los mismos cinco fallos conocidos de base/entorno y 59 omitidas, 351 segundos. Sin fallos heredados nuevos.


### GPU-guarded native depth draws

The native probe now generates a complete indirect command per eye from the GPU-verified lookup, then consumes those commands in the private translated depth draws. Unchanged reference draws retain the direct path and the complete original descriptor set is restored. Compute writes are ordered before indirect reads. Borrowing the argument ranges requires the same device, command, recording, eye and source generation; no CPU wait or pixel upload is introduced.

The actual game passed on the first attempt: original/private changed masks 815/817/817, horizontal centroids 575.461 / 573.630 / 577.313, vertical centroids 517.537 / 517.551 / 517.565. Both indirect commands contained 1344 elements, all 13,568 camera bytes validated, unchanged controls had zero differences and resources retired. Four aliases per eye, no uncovered entry and no alias/lookup error. The root component allocated 50,391,648 bytes, including the argument storage. Game-free lifecycle checks also cover stale/wrong command borrowing and both indexed/nonindexed geometry. The RTX 2060 SUPER authored GPU smoke verifies a negative indexed vertex offset, as well as zero-count rejection for corrupt camera data.

This establishes guarded command consumption in one reviewed same-state depth draw. The game shader still uses its reviewed private uniform descriptors; the authored lookup shader consumes the private root. Global-reading game shaders are not bound to private roots. See the separate [global read inventory](vr-private-root-read-inventory.md) for the remaining field and alias review.

Castellano: el diagnÃ³stico ya genera y consume un comando indirecto por ojo despuÃ©s de verificar su cÃ¡mara en GPU. Las referencias conservan su dibujo directo y se restaura el conjunto completo de descriptores. Hay barrera entre escritura de cÃ³mputo y lectura de argumentos; dispositivo, comando, grabaciÃ³n, ojo y generaciÃ³n deben coincidir.

Primera prueba real correcta: mÃ¡scaras 815/817/817, 1344 elementos por ojo, 13.568 bytes completos verificados, referencias sin diferencias y recursos liberados. Cuatro alias por ojo y ningÃºn error; 50.391.648 bytes de reserva en este componente. La prueba GPU propia tambiÃ©n conserva un desplazamiento negativo de vÃ©rtices indexados y devuelve cero elementos si se alteran los datos. Sigue siendo un dibujo aislado de profundidad. No se han conectado todavÃ­a los shaders globales del juego a las raÃ­ces privadas ni se ha completado la escena con color.

Final guarded-draw checkpoint: 103 VR tests pass; inherited regression 358 passed / 5 known baseline failures / 59 skipped in 342 seconds. The actual guarded game run peaked at 6621 MiB; a separate default-off 20-second startup passed at 4715 MiB, XR disabled. All 19 stable-install hashes match. No complete-scene or headset performance improvement is claimed.

Checkpoint final: 103 pruebas VR correctas; regresiones 358 correctas / 5 fallos conocidos / 59 omitidas en 342 segundos. Pico de la prueba real 6621 MiB; arranque normal separado de 20 segundos correcto y XR desactivado. Los 19 hashes estables coinciden. Sin afirmar mejora de escena completa ni rendimiento en visor.
