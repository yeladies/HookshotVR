# Native stereo implementation gates / Condiciones del estéreo nativo

## English

The latest human test found no perceptible fix: borders remain during head turns, temporal stereo is mostly black, HD still looks poor, and HUD/text sit too close to the lens edges. Audio works. The previous capture-delivery changes are not an accepted solution. Do not request another headset test of the same path as evidence of native stereo.

The production target is one simulation update and two complete eye renders from the same immutable submitted state. Each pair needs the same simulation/state identity, tracking-origin generation and pose sample/display-time prediction. Each eye has its own view and asymmetric projection. Two images from successive presents, duplicating a mono image, or shifting a completed picture do not satisfy this target. Same-state rendering removes the temporal difference between eyes; it does not promise zero end-to-end latency.

`vr/analyze_lineage.py` now emits `same_state_plan`, an ordered review of the presented resource-dependency slice. Camera dependence is recomputed from seeds and image/buffer producer edges instead of trusting recorded flags. Mismatches block replay. The review separates candidate shared work from eye-dependent graphics/compute/replacements and lists destination resources, external roots and unseeded graphics. The output is never replay authorization. Imported roots include ancestors of shared work too: an absent producer cannot prove eye independence. No shader/resource IDs from actual captures are published.

Private captured-frame observation: 1178 recorded events, 879 in the presented slice, 775 eye-dependent and 104 candidate shared events, 49 image destinations and five buffer destination ranges. There were zero dependency-flag mismatches and 79 unseeded dependent graphics operations. These counts are one capture, not whole-game coverage. Unseeded graphics are not automatically HUD: attachment blending makes overlays inherit scene dependence. Imported resources, opaque buffers, subresource/alias coverage, visibility and temporal histories remain blockers.

Implementation sequence:

1. Establish complete copy/clear/subresource and buffer coverage for a bounded scene pass. Preserve unchanged controls and submitted-resource generations. Review shader fields outside the patched matrix prefix. Run simulation and shared side-effecting compute once; preserve original ordering when shared work is interleaved.
2. Clone complete eye-dependent producer/consumer resources and histories, bind each eye's camera, and verify actual color/depth outputs from the same state. A diagnostic D32 lens shift of one draw is already validated; full-scene physical parallax is not.
3. Keep final eye images on GPU and copy/resolve directly into acquired OpenXR swapchain images. Submit both eyes atomically after their GPU dependencies, with their actual render poses/FOV. Remove CPU readback/upload from the production path. Never relabel an old render with a newer pose. Measure CPU/GPU render time and source-to-submit age separately from headset refresh.
4. Identify HUD/text/menu draws by verified render provenance. Render them separately, preserving the original layout, then compose a centered readable panel inside both eyes' visible FOV. Start with a configurable comfortable angular size and fit the complete original UI, including subtitles and menus; keep world FOV independent. Do not crop the completed image or guess UI solely from resolution or missing perspective matrices. Verify alpha and UI effects before suppressing original overlays.
5. Validate hidden-area coverage, culling, disocclusion, transitions, independent histories, lifetime and performance locally. Request Quest testing only after complete same-state eye images and stable GPU submission exist. Test slow/fast turns, near/far parallax, HUD corners, subtitles, inventory, audio and frame timing. Do not relax freshness gates to conceal stale frames.

## Castellano

La última prueba no mostró una mejora perceptible: siguen los bordes al girar, los negros en estéreo y los textos en los extremos. El audio funciona. Los cambios de capturas no son una solución aceptada.

El objetivo es actualizar la simulación una vez y renderizar los dos ojos desde el mismo estado inmutable, con identidad de estado, origen de tracking y muestra de pose comunes, y cámara/proyección propias de cada ojo. Esto elimina la diferencia temporal entre ojos, pero no garantiza latencia total cero.

El analizador ya genera un plan ordenado de dependencias de la imagen presentada. Recalcula la dependencia de cámara, detecta discrepancias y enumera recursos privados, entradas externas y dibujos sin cámara propia. Nunca autoriza por sí solo un replay. En una captura real privada: 1178 operaciones observadas, 879 en la cadena presentada, 775 dependientes del ojo, 104 compartidas candidatas, 49 destinos de imagen y cinco rangos de buffer. Hay 79 dibujos dependientes sin cámara propia; no se pueden identificar automáticamente como HUD. Siguen pendientes cobertura de copias/clears/alias, buffers opacos, visibilidad e historiales.

El desarrollo debe completar primero un pase y sus recursos, después los dos ojos de la escena, y luego el envío GPU directo a OpenXR sin lectura CPU. Se conserva la pose real de renderizado y ambos ojos se publican juntos. HUD, textos y menús necesitan identificación por su origen de render y una composición independiente centrada, con el diseño original completo dentro del campo visible. No se resuelve recortando la imagen final ni reduciendo el FOV del mundo. Las pruebas en Quest vendrán después de validar esa ruta; no se pide repetir los perfiles temporales anteriores.
