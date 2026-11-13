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
