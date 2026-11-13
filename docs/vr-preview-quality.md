# Preview quality and continuity / Calidad y continuidad de la prueba

## English

The first Quest comparison reported black intermittency in temporal stereo, visible rectangular black edges in both modes and low sharpness in mono. The archived reports observed a source lens near 108° horizontal / 110° vertical. These are source projection angles, not a measurement of physical headset FOV. The old transport was fixed at 1280×720 per eye.

The new opt-in profiles add 1920×1080 transport for immersive views and a bounded source-lens margin. A 25% margin scales the tangents of the runtime envelope; it does not add 25% to angles or stretch the captured picture. The game perspective/frustum field path receives the wider lens, and the submitted projection retains its observed source FOV. This margin needs human verification for edges and comfort. The HD profile also requests the existing fixed 100% render-resolution override (`HOS_DYNRES=0`); game sizing/quantization and actual performance must be measured separately. It can reduce game frame rate on the development PC.

```powershell
# First compare mono sharpness and edge coverage:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -WideFov -PreviewResolution 1080p -FovMarginPercent 25 -Audio -Seconds 180
# Separate temporal-stereo comparison, keeping the cheaper transport:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-immersive-preview.ps1 -TemporalStereo -PreviewResolution 720p -FovMarginPercent 25 -Audio -Seconds 180
```

Complete stereo pairs are now committed atomically. A lone new eye, temporarily excessive present gap or unassociated source image cannot replace a still-fresh accepted pair. A held pair keeps both original image/pose identities; age budgets are not refreshed. The limits remain 200 ms per received image, 300 ms per capture pose and four source presents between eyes. A menu, dimensions change or tracking-origin change cannot reuse the old pair. Diagnostics distinguish commits, held requests, unavailability, excessive gaps, expired poses/images and maximum evaluated pose age. This reduces one source of avoidable blanks; it does not make slow capture fresh or solve temporal stereo's inherent limitations.

Each future headset launcher archives its native stdout/stderr and camera report within its unique private run directory. The previous stereo log had already been overwritten by the subsequent mono test, so its precise blackout cause cannot be reconstructed retrospectively.

### Local validation

All 94 VR tests pass, including mono/stereo HD swapchain dimensions, upload offsets, separate eye regions, margin math, atomic pair retention, age/origin rejection and launcher guards. Actual offscreen mono HD with a 25% margin passed at 6618 MiB process peak. Actual offscreen 720p temporal stereo produced distinct accepted pairs and exited normally. The synthetic lens changed from 110° horizontal / 100° vertical to approximately 121.5° / 112.3°, matching its requested margin.

The HD temporal-stereo actual-game test did **not** pass the pair-freshness gate; it is not an accepted headset profile. A subsequent 720p diagnostic snapshot recorded 124 commits and 13 unavailable requests, including 8 expired-pose checks and 6 expired-image checks; the maximum evaluated pose age was 701 ms and was rejected by the unchanged limits. These are source-side observations, not headset FPS or blackout-duration measurements. Human confirmation of improved edges, sharpness and continuity remains pending. The menus and full same-state shaded stereo remain separate work.

## Castellano

La primera comparación en Quest mostró intermitencias negras en estéreo temporal, bordes negros rectangulares en ambos modos y poca nitidez en mono. Los informes archivados observaron una lente fuente próxima a 108° horizontal / 110° vertical; no es una medición del FOV físico del visor. La captura anterior estaba fijada a 1280×720 por ojo.

Los nuevos perfiles permiten transporte de 1920×1080 y un margen óptico acotado. El 25% amplía las tangentes del FOV solicitado por el runtime: no suma un 25% a los ángulos ni estira una captura. Cambia la lente real del juego y conserva el FOV observado al enviarla. Falta comprobar en Quest los bordes y la comodidad. El perfil HD solicita además el ajuste existente de render fijo al 100%; la cuantización de tamaños del juego y el rendimiento se comprueban por separado. Puede bajar los FPS.

Los comandos anteriores separan mono HD para nitidez/cobertura y estéreo temporal 720p para continuidad. Las dos imágenes del último par válido se conservan juntas mientras sigan frescas; nunca se mezcla un ojo nuevo con otro fuera de los límites ni se renueva su antigüedad. Se mantienen 200 ms por imagen, 300 ms por pose y un máximo de cuatro presents entre ojos. Menús, cambio de tamaño u origen invalidan el par anterior. Los contadores distinguen los motivos de descarte. Conservar un par fresco evita vacíos innecesarios, pero no convierte una captura lenta en una imagen actual.

Pasan 94 pruebas VR. El ensayo real sin visor de mono HD con margen pasó, con pico de proceso de 6618 MiB. El de estéreo 720p produjo pares distintos aceptados y cerró normalmente. El FOV sintético pasó de 110° / 100° a unos 121.5° / 112.3°, conforme al margen.

El ensayo real de estéreo HD no pasó la frescura y no se da por listo para el visor. En una instantánea del ensayo posterior 720p se registraron 124 commits y 13 solicitudes sin par válido, con descartes por poses e imágenes antiguas. La máxima pose evaluada, de 701 ms, se rechazó. No son medidas de FPS ni de duración de los negros en Quest. Cada prueba futura conservará sus registros completos. Siguen pendientes la comprobación humana, los menús y el estéreo completo de la escena desde un mismo estado.

Final build: ABI20 source `748b4bb4`. All 94 VR tests pass. The inherited suite reports 357 passed, the same 5 baseline/environment failures and 59 skipped (341 seconds). Default-off startup reached its bounded time limit at 3032 MiB, XR disabled; all 19 monitored stable-install hashes match. Private quality launchers and receipts are prepared. The updated headset visual check is pending.

Build final: ABI20 `748b4bb4`, 94 pruebas VR correctas; regresiones 357 correctas, los 5 fallos conocidos y 59 omitidas. Arranque normal sin XR y 19 hashes estables intactos. Lanzadores y recibos privados preparados; falta la comprobación visual actualizada en Quest.

## Follow-up: resolution and capture delivery / Resolución y entrega

The second headset check improved coverage but still exposed rectangular borders during head turns. HD remained visibly pixelated, temporal stereo flickered and was mostly black, and audio worked correctly. These results supersede any assumption that the earlier quality profiles solved the visual defects.

The HD launcher had forced handheld mode, where the host ignores `HOS_OUTPUT_RES`. The corrected isolated HD mode uses docked operation, requests 1920x1080 and disables dynamic resolution. New diagnostics record the observed world render target and its matched camera dimensions; the output/swapchain size alone does not prove scene resolution.

An optional `-LowLatencyPreview` path submits the current capture and waits up to 20 ms for its GPU timeline. A completed transfer can be delivered immediately instead of waiting for a later present. Timeout keeps the existing deferred ownership path; a GPU error never publishes unreadied memory. This may cost frame rate and is still CPU readback/upload, not zero-copy. It does not relax image/pose freshness limits or make temporal eyes simultaneous.

Pair statistics now survive session cleanup until the closing log is written. Earlier zero-valued closing counters were erased diagnostics, not proof that no pairs had been shown. The new source has 95 passing VR tests; the inherited suite reports 358 passed, the same five baseline/environment failures and 59 skipped. Native Windows compilation succeeds. Headset improvement remains unverified.

La segunda prueba en Quest mejoró la cobertura, pero seguían apareciendo bordes rectangulares al girar. HD continuaba pixelado; el estéreo temporal parpadeaba y permanecía casi siempre negro. El sonido funcionó correctamente. Los perfiles anteriores no resolvieron los defectos visuales.

El lanzador HD forzaba el modo portátil, donde el host ignora `HOS_OUTPUT_RES`. El modo HD aislado corregido utiliza el modo sobremesa, solicita 1920x1080 y desactiva la resolución dinámica. Los nuevos diagnósticos registran el target real observado del mundo y las dimensiones de su cámara; el tamaño de salida por sí solo no demuestra resolución interna.

La opción `-LowLatencyPreview` espera como máximo 20 ms a la transferencia GPU de la captura actual para poder entregarla sin esperar otro present. Si vence el plazo conserva la ruta diferida; un error GPU nunca publica memoria sin completar. Puede reducir los FPS, sigue usando lectura CPU y no convierte los ojos temporales en simultáneos ni relaja la frescura.

Los contadores de pares se conservan hasta escribir el registro de cierre. Los ceros anteriores procedían del borrado de estadísticas. Pasan 95 pruebas VR; la suite heredada da 358 correctas, los mismos cinco fallos conocidos y 59 omitidas. Compila en Windows. Falta validar la mejora visual en Quest.

Local follow-up measurements (ABI20, source `2e68d372`): HD mono passed camera/FOV verification with observed world target and camera both 1920x1080, peak process memory 5374 MiB. At the last periodic capture counter, 796 transfers completed immediately and 44 deferred, with no transfer errors. Temporal 720p passed the distinct-pair check and exited normally: 120 accepted pairs were logged; the last periodic diagnostics had 111 commits and five requests without a valid pair. Its observed world target was 1152x648. These are synthetic-pose, non-XR checks, not headset frame-rate or black-screen-duration measurements. The menu-transition check still failed. All 19 monitored stable-install hashes match.

Medidas locales (ABI20, `2e68d372`): mono HD pasó cámara/FOV con target del mundo y cámara a 1920x1080, pico de proceso 5374 MiB; el último contador periódico mostró 796 transferencias inmediatas, 44 diferidas y ningún error. Estéreo 720p pasó la comprobación de pares distintos, registró 120 pares aceptados y cerró normalmente; el último diagnóstico periódico tenía 111 commits y cinco solicitudes sin par válido. Su target del mundo era 1152x648. Son ensayos sin visor con poses sintéticas, no medidas de FPS ni de duración de negros en Quest. La transición de menús sigue sin pasar. Coinciden los 19 hashes vigilados de la instalación estable.
