# Native stereo progress: translated views and scene dependencies

## English

The latest playable Quest retest did not resolve black stereo frames, visible image borders or unreadable HUD placement. Audio was correct. This work advances independent native components and does not label the temporal preview as fixed.

### Verified translated depth draw

The isolated `-TranslatedDrawProbe` uses a distinct run/build-scoped selection protocol and the same submitted GPU camera object for both private views. Two unchanged reference draws must still match the original exactly. Two additional draws translate the view, recompute VP/inverse-view and preserve projection and unknown bytes. The source simulation is never repeated. This is one selected draw, not full-scene stereo or calibrated IPD.

The actual-game depth test with a 0.01-unit diagnostic displacement passed on its first attempt: 817 original changed pixels; private masks 819/813; horizontal centroids 575.497 / 573.591 / 577.351; vertical centroids 517.558 / 517.554 / 517.630. Both unchanged controls had zero differences. Both complete 13568-byte payloads validated, source bytes remained exact and all resources retired. Peak process memory 6588 MiB. No XR session opened.

Earlier 2- and 8-unit tests exhausted eight attempts without passing: private views produced no visible changed fragments. These are retained as negative/inconclusive evidence. The default is now the smaller observed passing displacement. It is not a measurement of game metres or the user's IPD. Larger supported diagnostic magnitudes are not offered as headset settings.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -IsolatedDrawProbe -TranslatedDrawProbe -SnapshotStableSaves -LoadTestSave -Seconds 150
```

### Experimental color gate

`-TranslatedColorDrawProbe` has a separate protocol and requires actual primary color output. It keeps the read-only vertex/fragment shader inventory, descriptor, alias, target, lifetime and unchanged-image controls. It can review primary camera reads confined to the matrix prefix plus four scalar offsets whose submitted values must satisfy near/far/projection and viewport identities. Programs with unresolved reads or any other preserved-field reads remain rejected. Value relationships are a bounded diagnostic condition, not general proof of all shader semantics or permission for scene replay.

The GPU shader checks the near/far relationship against the actual projection, the clip interval span, and the actual target size/reciprocals before transforming. A bad scalar or viewport leaves the copied payload unchanged. CPU verification independently checks these relationships and all preserved bytes. Own-data GPU positive and invalid-viewport rejection tests pass on RTX 2060 SUPER. Actual-game color validation is recorded separately; no complete shaded scene is claimed here.

### Transfer coverage and validation cost

The native texture transfer path now records emitted image copies, blits, resolves, buffer/image copies and clears in the conservative dependency graph. Clear and partial-write events retain the previous destination. No full-overwrite or subresource completeness is inferred. CPU upload fallbacks, aliases and other internal passes remain review requirements.

One private actual frame's presented slice contained 905 graphics, 61 compute, three transfer and 31 clear events, with zero reconstructed dependency-flag mismatches. The inventory had 50 candidate private images, approximately 78.2 MB of image-allocation references and 69.6 MB of buffer-range references per eye. These are one-frame planning references, not actual total allocations or whole-game coverage.

Submitted-camera matching now checks the immutable copied block's shape/coherence once per call before comparing up to 16 separately checked host observations. Freshness, ambiguity, full prefix validity and source dimensions remain enforced. Tests cover incoherent VP, nonfinite trailing fields, truncated blocks, all 16 equivalent identities and scope isolation. This removes redundant validation; headset frame-rate improvement has not been measured.

### HUD fitting

`vr/src/hud_panel_fit.h` fits the complete original UI aspect inside both runtime eye frusta with an inward safety margin and an angular size limit. It accounts for IPD, asymmetric lenses and eye rotation in a head-relative panel space, and rejects invalid tracking, poses, lenses and panels behind an eye. Tests independently project all corners using the existing camera transform. The helper does not isolate game UI, compose a layer or modify the world FOV. HUD extraction/composition remains pending.

## Castellano

La última prueba en Quest no solucionó los negros, los bordes ni los textos en los extremos; el audio funciona. Estos avances nativos no se presentan como una corrección del preview temporal.

La prueba real de un dibujo de profundidad con vistas trasladadas pasó al primer intento con desplazamiento diagnóstico de 0.01 unidades: 817 píxeles originales, máscaras privadas de 819/813, centros horizontales 575.497 / 573.591 / 577.351 y verticales 517.558 / 517.554 / 517.630. Las dos referencias coincidieron exactamente; se validaron los 13568 bytes completos de cada cámara, la fuente quedó intacta y se liberaron recursos. Pico de proceso 6588 MiB. No se abrió XR ni se repitió la simulación. Sigue siendo un dibujo aislado, no la escena ni una IPD calibrada. Los ensayos anteriores de 2 y 8 unidades no pasaron y se conservan como evidencia negativa.

El modo experimental con color tiene protocolo propio y exige salida de color real. Solo permite matrices y cuatro campos escalares cuyos valores GPU deben ser coherentes con proyección, planos de recorte y tamaño del target. Campos no revisados o lecturas sin resolver siguen bloqueados. Pasan las pruebas GPU propias positiva y de rechazo por viewport inválido. No se afirma que la escena completa esté renderizada en estéreo.

Se añadieron al grafo las transferencias y limpiezas emitidas por la ruta nativa de texturas, conservando de forma prudente las dependencias anteriores del destino. Una captura real incluyó 905 dibujos, 61 cómputos, tres transferencias y 31 limpiezas en la cadena presentada, sin discrepancias de dependencia. No es cobertura completa del juego.

La validación de un mismo bloque de cámara ya no se repite para cada observación candidata; se mantienen frescura, identidad, coherencia y ambigüedad. Falta medir el efecto real en el visor.

El nuevo cálculo del HUD conserva su aspecto completo y comprueba que sus esquinas queden dentro de los campos de ambos ojos, con margen, IPD y lentes asimétricas. No altera el FOV del mundo. Todavía falta extraer y componer la interfaz real del juego. No hay una nueva prueba del visor solicitada: queda pendiente hasta que el usuario confirme disponibilidad.

### Actual color result and checkpoint validation

The first color run could not select the live eligible program because the 64-program audit inventory had filled with rejected depth/mesh work. Color-mode auditing now captures live-preflight-eligible programs within the same bound. A subsequent run recorded one color attempt; the original emitted no visible changed pixels. A bounded retry policy then tried eight eligible draws: each remained invisible in the original and private outputs. Camera payloads validated, source bytes stayed exact, controls had zero mismatches and resources retired, but color stereo remains unverified. Repeated invisible draws are not counted as successful stereo. Mismatched references, unreviewed shaders or invalid camera payloads never become retryable through this policy.

Final native checkpoint: 99 VR tests pass. The inherited suite reports 358 passed, the same five known baseline/environment failures and 59 skipped (322 seconds). Native Windows compilation succeeds. All 19 monitored stable-install hashes match. No new headset session was opened and no headset test is requested while the user is unavailable. The complete game scene, mesh/global-memory camera routing and separate HUD/OpenXR composition remain pending.

Resultado real de color: el cupo de auditoría inicialmente impedía seleccionar el dibujo elegible; el modo con color ahora reserva ese cupo para programas que pasan el preflight, sin ampliarlo. Un primer intento y después ocho reintentos acotados siguieron sin modificar píxeles visibles del original. Se validaron las matrices, la fuente quedó intacta, las referencias tuvieron cero diferencias y se liberaron recursos, pero el estéreo con color sigue sin validar. No se convierten ensayos invisibles en éxitos. Pasan 99 pruebas VR; regresiones 358 correctas, los cinco fallos conocidos y 59 omitidas. Los 19 hashes estables coinciden. No se ha abierto XR. Continúan pendientes escena completa, cámaras en memoria global/shaders de malla y composición separada del HUD.
