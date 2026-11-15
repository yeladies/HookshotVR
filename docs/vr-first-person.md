# First-person camera development / Cámara en primera persona

## English

This is the opt-in first-person work on `vr/camera-rebuild`. The old depth camera remains retired. A first-person desktop capture and a native stereo headset build are separate milestones; completing the former does not demonstrate the latter.

The rig uses the guarded player root transform plus the eye midpoint of the local model's bind pose. Animated head bones are not sampled for camera motion. Child and adult heights are read from the user's own FRES files by `vr/first_person_assets.py`; the generated profile under ignored `work/` is private game-derived data and must never be uploaded. The reader accepts only validated little-endian FRES 10.4 skeletons, checks parent chains, and rejects ambiguous or malformed models. Format facts were checked against [BfresLibrary's skeleton reader](https://github.com/KillzXGaming/BfresLibrary/tree/master/BfresLibrary/Shared/Models/Skeleton); no library source or assets are bundled.

OpenXR head and eye poses remain absolute. Initial yaw aligns the head's horizontal direction to Link's facing direction; initial pitch and roll remain intact. Subsequent body rotation does not turn the view automatically. The root, tracking reference, yaw and metre scale are explicit inputs. Physical movement and the already-located eye offsets are scaled once. A recenter, actor change or age/height change establishes a new reference. There is no smoothing or animated bob in this module.

One game present owns one immutable packet. A bounded 64-present CPU history lets a GPU consumer request the matching older pose; recentering clears old-origin entries. This does not replace GPU resource leases/fences. Repeated engine queries cannot replace its tracking sequence, root or timestamp; a recenter invalidates that ownership. The internal renderer consumer builds both asymmetric projections and the conservative stereo visibility planes from that frozen pose and explicit native near/far values.

The host-renderer contract is ABI 21. A valid packet supplies the center-head view, both eye views, runtime FOV angles, tracking sequence, origin generation and original observation timestamp. The selected main view receives its matrix and known float/double eye/target/up caches from that one packet. Other cameras and debug free-camera mode retain their own behavior. A stale or invalid tracking observation is rejected; the module does not relabel it as a fresh frame.

Local non-XR diagnostic (requires a locally built game and private save copy):

```powershell
powershell -File vr/run-game.ps1 -FirstPersonReadbackTest -FirstPersonSweep None -SnapshotStableSaves -LoadTestSave -Seconds 150
```

Use `-FirstPersonSweep Yaw`, `Pitch`, `LookDown`, `Lean`, `WideYaw` or `WidePitch` for an explicitly synthetic tracking sweep. These tests never open OpenXR. Their screenshots and profile stay under ignored `work/`. The optional `-FirstPersonMoveTest` applies two short opposite stick inputs to the private save after gameplay starts. `-FirstPersonKeepHead` is an explicit comparison without the geometry mask.

The default mask degenerates only triangles influenced by the Head subtree, in every LOD. Vertex data, sizes and counts are preserved. Models are generated in an isolated overlay and the run must log their actual loading. This temporary asset mask affects head shadows and cutscenes too; a render-only mask remains a release task.

The current `1` world unit per metre is an explicit working assumption, consistent with the model/controller audit, and still requires physical headset calibration.

Head visibility, native full-scene per-eye rendering, projection/culling ownership, history separation and XR presentation must be checked independently. No native stereo acceptance is implied by a successful desktop capture.

## Castellano

Este trabajo de primera persona es optativo y está en `vr/camera-rebuild`. La cámara anterior por reconstrucción de profundidad sigue retirada. Una captura de escritorio en primera persona y una build estéreo nativa para el visor son hitos distintos.

El rig usa la transformación comprobada de la raíz de Link más el punto medio de los ojos en la pose de referencia del modelo. La cámara no sigue los huesos animados de la cabeza. El lector obtiene las alturas de Link niño y adulto de los archivos FRES locales del usuario. El perfil generado dentro de `work/` es privado, contiene datos derivados del juego y no debe publicarse. Se valida la versión, los huesos, sus padres y las transformaciones; se rechazan los modelos ambiguos o malformados.

Las poses de cabeza y ojos de OpenXR permanecen absolutas. El yaw inicial alinea la dirección horizontal del visor con Link y conserva la inclinación inicial. Después, el giro automático del cuerpo no arrastra la vista. La raíz, la referencia de seguimiento, el yaw y la escala se mantienen separados. El desplazamiento físico y el IPD ya incluido en cada ojo se escalan una sola vez. Un recentrado o cambio de actor o altura establece una referencia nueva. El módulo no añade suavizado ni balanceo de animación.

Se conservan hasta 64 paquetes CPU para que el renderer pueda recuperar la pose de un fotograma anterior; el recentrado invalida el origen antiguo. Esto no sustituye a los fences y préstamos de recursos GPU. Cada fotograma conserva un paquete inmutable: las consultas repetidas no cambian su pose, raíz ni marca temporal. El consumidor del renderer calcula ambas proyecciones asimétricas y los planos de visibilidad de ambos ojos desde ese paquete, recibiendo los near/far nativos explícitamente.

El contrato entre host y renderer pasa a ABI 21. Un paquete válido contiene la vista central, las dos vistas de los ojos, los ángulos FOV, la secuencia, la generación de origen y la marca temporal original. La cámara principal recibe de ese paquete sus filas de vista y sus cachés de posición, objetivo y vertical. Las otras cámaras y la cámara libre de depuración conservan su funcionamiento. Las observaciones caducadas o inválidas se rechazan sin presentarlas como nuevas.

El comando anterior realiza una prueba privada del juego real sin abrir OpenXR. `Yaw`, `Pitch`, `LookDown` y `Lean` generan movimientos sintéticos explícitos. `-FirstPersonMoveTest` añade dos entradas cortas y opuestas de movimiento al guardado privado. Las capturas y el perfil quedan en `work/`. Una unidad por metro sigue siendo una hipótesis de trabajo y necesita calibración física en el visor.

La máscara temporal elimina triángulos vinculados a los huesos de la cabeza en todos los niveles de detalle y conserva vértices, tamaños y recuentos. El registro debe confirmar la carga real del modelo desde el overlay. También afecta a sombras y escenas de vídeo, por lo que una máscara solo de renderizado sigue siendo tarea de publicación. `-FirstPersonKeepHead` permite la comparación explícita con cabeza visible.

La escena completa por ojo, la autoridad sobre proyección/descartes, los historiales y la presentación XR se verifican aparte. Una captura de escritorio correcta no acredita estéreo nativo.

## Verified milestone — 2026-11-15

Native SDK source `3f307691` compiles without errors (82 seconds plus a 6-second host link). Six isolated real-game diagnostics have been completed across the incremental first-person builds: stationary, looking down, smooth yaw, physical translation combined with actual player motion, wide yaw and wide pitch. The last two use `-FirstPersonSceneAudit` and match the new view to actual native scene/submission camera data. Wide yaw covered 2.791 radians (about 160 degrees); vertical direction ranged from approximately -0.932 to +0.932. The model overlay is confirmed loaded in the game log. Screenshots were inspected locally and remain private. Child gameplay is exercised; adult profile/mask generation is validated, with adult gameplay still untested.

The root-motion diagnostic moved Link by 1.125 game units horizontally. The independent synthetic HMD displacement spans approximately 0.4 / 0.2 / 0.3 game units on X/Y/Z at the explicit working scale. These are game-space measurements, not a physical headset calibration.

After correcting a stale recorder fixture and rebuilding every fixture executable, the complete VR suite passes 169 cases. The earlier 167-case run did not establish fresh recorder-fixture coverage; the actual game diagnostics used newly built native binaries. The complete staged inherited suite passes 387, with five known environment failures and 30 skipped (228 seconds). A repeated native root-alias test verifies the isolated GPU diagnostic and exports its private camera payload after the recorder-order fixes (peak working set 6781 MiB). A separate 20-second default-off run opens no XR session or immersive camera (3086 MiB). All 19 stable-installation manifest hashes remain unchanged.

The current game screenshots are center-head desktop renders. Complete native per-eye scene rendering, original projection/culling ownership, temporal resources, HUD composition and headset acceptance remain pending. No headset test was opened during these checks and no correction of the old XR blackouts is claimed.

## Hito comprobado — 15-11-2026

La build SDK `3f307691` compila sin errores: 82 segundos y otros 6 para enlazar el host. Se han completado seis pruebas aisladas del juego real a lo largo de las builds incrementales: reposo, mirar abajo, giro suave, desplazamiento físico combinado con movimiento real de Link, giro horizontal amplio y giro vertical amplio. Las dos últimas usan `-FirstPersonSceneAudit` y relacionan la vista nueva con los datos de cámara de los dibujos y envíos nativos. El recorrido horizontal cubre unos 160 grados; la componente vertical de la dirección recorre aproximadamente de -0,932 a +0,932. El registro confirma la carga del modelo desde el overlay y las imágenes revisadas quedan privadas. Se ha probado Link niño dentro del juego; para adulto se validan la generación de perfil y máscara, pero falta la prueba en juego.

Link se desplazó 1,125 unidades del juego en horizontal. El movimiento independiente simulado del visor abarca aproximadamente 0,4 / 0,2 / 0,3 unidades en X/Y/Z con la escala provisional explícita. Estas cifras no son una calibración física en metros.

Tras corregir un fixture del recorder que había quedado sin recompilar y reconstruir todos los ejecutables, pasan las 169 pruebas VR. La ejecución anterior de 167 casos no acreditaba la comprobación fresca de ese fixture; las pruebas del juego real sí usaron binarios nativos nuevos. La batería heredada completa, con los archivos nuevos preparados para publicar, da 387 correctas, cinco fallos conocidos del entorno y 30 omitidas (228 segundos). Se repite con éxito el diagnóstico GPU de aliases de raíz tras corregir su ordenación: exporta el payload privado y alcanza 6781 MiB. El arranque normal de 20 segundos mantiene XR y la cámara inmersiva desactivados, con 3086 MiB. Los 19 hashes de la instalación estable siguen intactos.

Las capturas actuales son renders de escritorio de la vista central de la cabeza. Siguen pendientes la escena completa por ojo, la autoridad sobre proyección y descartes, los recursos temporales, la composición del HUD y la aceptación en visor. Estas pruebas no abrieron las Quest ni acreditan corregidos los apagones de la ruta XR antigua.

## Submission and temporal-camera audit — English

The follow-up native SDK build `411f9102` compiles with zero failures. A new 150-second, non-XR first-person yaw diagnostic records 136 first-person samples and 32 consecutive private camera payloads with 32 distinct views. All 32 have a source NVN submit identifier and consistent matching observation tags; none has an ambiguous observation present. Peak working set is 6661 MiB. This is an observer-consistency result, not proof that a present counter is the producer's simulation tick.

`-FirstPersonHistoryAudit` enables `-FirstPersonSceneAudit` and saves at most 32 immutable submission-overlay copies of one fixed program, descriptor size and viewport. Its marker is armed only after first-person gameplay is observed. Copies and reports remain private under ignored `work/`. The analyzer rejects stationary evidence, incoherent VP matrices, changed capture identity, partial provenance and malformed paths. Run it locally:

```powershell
python -I -B vr/audit_camera_history.py <private-camera-history-directory> --out <private-report.json>
```

Movement distinguishes current view/VP/inverse-view data from one-sample-old aliases. Projection remains unchanged in this yaw experiment, so its temporal role cannot be inferred from equal values. The tool reports numerical candidates only: no shader-consumer, producer-history or patch-safety claim. Submission-overlay bytes are not GPU-completed readback.

The source NVN submit serial is retained separately from a merged Vulkan recording's timeline value. Matching observations that disagree about a present report ambiguity rather than selecting a convenient tag. The host's first-person observation also retains the exact present supplied to its rig query. Neither change creates a producer-owned frame identifier; that integration remains a gate before full-scene stereo.

The frozen renderer packet retains the original OpenXR LOCAL-space head/eye poses in metres separately from the game-world matrices. Future XR projection-layer submission must use those original poses from the rendered packet, not game-world positions or newly located poses. Each eye also needs its own previous-render history and fenced writable resources. This audit does not enable a headset launcher or modify guest camera buffers.

## Auditoría temporal y de envíos — Castellano

La siguiente build SDK, `411f9102`, compila sin fallos. Una prueba nueva de giro de 150 segundos, sin XR, registra 136 muestras de primera persona y 32 payloads privados consecutivos con 32 vistas distintas. Los 32 conservan el identificador de envío NVN y marcas coherentes entre las observaciones coincidentes; ninguno presenta ambigüedad. El máximo de memoria de trabajo es 6661 MiB. Esto comprueba coherencia entre observadores, pero no demuestra que el contador de presentaciones identifique el paso de simulación del productor.

`-FirstPersonHistoryAudit` activa también `-FirstPersonSceneAudit` y guarda hasta 32 copias inmutables del overlay de envío para un programa, tamaño y viewport fijos. Se arma después de detectar primera persona en juego. Los payloads y sus informes quedan privados en `work/`. El analizador rechaza capturas sin movimiento, VP incoherentes, cambios de identidad, metadatos incompletos y rutas incorrectas. El comando anterior permite ejecutarlo localmente.

El movimiento permite distinguir vistas, VP e inversas actuales de aliases de la muestra anterior. La proyección permanece constante durante este giro y sus valores iguales no permiten establecer su papel temporal. El informe contiene candidatos numéricos: no acredita consumidores de shaders, semántica del productor ni permiso para parchear. Las copias del overlay tampoco son lecturas GPU completadas.

El número de envío NVN se conserva separado del valor temporal de una grabación Vulkan que puede agrupar varios envíos. Si las observaciones coincidentes discrepan en la presentación, se informa de la ambigüedad. La observación de primera persona conserva el contador exacto entregado a la consulta del rig. Aún falta un identificador de fotograma propiedad del productor para habilitar estéreo de escena completa.

El paquete congelado del renderer conserva las poses originales de OpenXR en espacio LOCAL y metros, separadas de las matrices del mundo del juego. La futura capa de proyección XR debe usar esas poses del paquete renderizado. Cada ojo necesitará también su historial de render y recursos de escritura protegidos por fences. Esta auditoría no activa un lanzador para el visor ni modifica buffers de cámara del juego.

The independent translation sweep also passes: 133 first-person samples, 32 distinct camera payloads, and 6661 MiB peak working set. Its bounded graphics shader audit observes 64 programs and 104 primary-camera bindings, with all observed modules available. Of 84 static module/binding queries, 82 resolve their static subset and two remain unresolved. Static reads establish consumption of current camera regions in several stages, but do not establish the meaning of every previous-frame alias, dynamic reads or complete scene coverage. Raw modules and reports remain private.

El barrido independiente de desplazamiento también pasa: 133 muestras de primera persona, 32 payloads distintos y 6661 MiB de memoria máxima. La auditoría gráfica acotada observa 64 programas y 104 bindings de cámara principal, sin módulos ausentes. De 84 consultas estáticas, 82 resuelven su subconjunto y dos siguen incompletas. Hay lecturas de regiones actuales de cámara en varias etapas, pero faltan la semántica de cada alias anterior, los accesos dinámicos y la cobertura de escena completa. Los módulos e informes quedan privados.

## Native depth and writer-owned view tags — English

`vr_camera_build_native_projection` builds each eye's XY projection directly from its located FOV and preserves the source perspective's Z/W coefficients exactly. It rejects orthographic, oblique and noncanonical W matrices. The near/far reported by this adapter are the effective distances represented by the supplied matrix coefficients, not a recovery of the producer's original unrounded parameters. This avoids changing depth precision merely to reconstruct clip distances. Forward/reversed depth, NVN/Vulkan clipping, finite/infinite far and fail-atomic rejection are covered by the C tests.

The new in-game projection check exposed an observation bug: refreshing an existing view at submit time could label it with the current present counter rather than the packet that wrote its rows. The host now caches that writer-owned tag with the exact rows and camera identity. A refresh retains it only for identical rows; a changed camera, debug view or even a numerically close changed matrix invalidates it. First-person observations cannot fall back to the current counter. One submit freezes the selected tag for both its primary and uniform observations.

The regression test compiles the actual host observation functions against invented guest memory. It advances the counter between view creation and observation, and again between primary and uniform publication. It checks that the original writer tag survives, and that a changed view is rejected instead of borrowing a pose.

Native SDK source `d82b0666` builds with zero failures (200 seconds plus 7 seconds for the host link). A completed 150-second non-XR yaw run records 134 first-person samples with 6674 MiB peak working set. All 32 private captured camera blocks recover their frozen eye packet, match its center-head view, preserve exact native depth and match the tracking FOV. The eye separation is the authored synthetic fixture's 64 mm at the provisional scale; this is not a Quest IPD measurement or physical scale calibration. All 19 stable-installation hashes remain unchanged. The complete fresh VR suite passes 171 tests.

`-FirstPersonHistoryAudit` now requires this complete 32-sample projection check, in addition to collecting the payloads. Its private JSON reports the original tracking sequence/time and lookup status, without refreshing poses or selecting a nearby frame. The source NVN submit, Vulkan recording and observation tag remain distinct concepts. Producer simulation identity, complete scene stereo, shader-wide temporal semantics and headset acceptance remain unfinished.

The new `audit_camera_consumers.py` joins private numerical matrix candidates to conservative static reads from the corresponding module/binding observations. Its graphics capture finds current-view regions in several stages; it does not establish that previous-frame aliases are unused. Dynamic reads and unobserved compute/graphics programs still require review. The tool never modifies shaders or buffers and always reports `safe_to_patch: false`.

## Profundidad nativa y etiqueta del escritor — Castellano

`vr_camera_build_native_projection` construye la proyección XY de cada ojo directamente desde su FOV localizado y conserva exactamente los coeficientes Z/W de la perspectiva nativa. Rechaza proyecciones ortográficas, oblicuas o con W no canónico. Los near/far informados son los límites efectivos representados por esos coeficientes, no los parámetros originales antes de redondearse. Se evita así cambiar la precisión de profundidad por reconstruir los planos. Las pruebas C cubren profundidad normal/invertida, clipping NVN/Vulkan, far finito/infinito y errores sin salida parcial.

La comprobación dentro del juego detectó un fallo de observación: al refrescar una vista existente durante el envío, podía etiquetarse con el contador actual en lugar del paquete que escribió sus filas. El host conserva ahora esa etiqueta junto con las filas exactas y la identidad de cámara. Solo la retiene si las filas son idénticas; otra cámara, la cámara de depuración o un cambio pequeño de matriz la invalidan. Primera persona no puede recurrir al contador actual. Cada envío usa la misma etiqueta congelada para sus observaciones primaria y de uniformes.

La regresión compila las funciones reales del host con memoria del juego inventada. Hace avanzar el contador entre escritura y observación, y entre la observación primaria y la de uniformes. Comprueba que se conserva la etiqueta del escritor y que una vista distinta se rechaza sin pedir una pose cercana.

La build SDK `d82b0666` compila sin fallos: 200 segundos y 7 para enlazar el host. Una prueba completa de giro de 150 segundos sin XR registra 134 muestras de primera persona y 6674 MiB máximos. Los 32 bloques capturados recuperan su paquete de ojos, coinciden con la vista central, conservan la profundidad nativa exacta y corresponden al FOV del seguimiento. La separación de 64 mm pertenece al fixture sintético con escala provisional: no mide el IPD de las Quest ni calibra la escala física. Los 19 hashes estables siguen intactos y pasan las 171 pruebas VR recién compiladas.

`-FirstPersonHistoryAudit` exige ahora esa comprobación completa de 32 muestras. Su informe privado conserva secuencia, tiempo y resultado de búsqueda originales, sin renovar poses ni escoger fotogramas próximos. El envío NVN, la grabación Vulkan y la etiqueta de observación siguen siendo conceptos distintos. Faltan identidad de simulación del productor, escena estéreo completa, semántica temporal de todos los shaders y aceptación en visor.

`audit_camera_consumers.py` relaciona candidatos numéricos privados con las lecturas estáticas observadas en cada módulo y binding. La captura gráfica encuentra regiones de vista actual en varias etapas, pero no demuestra que los aliases anteriores no se utilicen. Siguen pendientes accesos dinámicos y programas compute/gráficos no observados. La herramienta no modifica shaders ni buffers y mantiene `safe_to_patch: false`.

The second native-depth run, synthetic translation plus compute audit, also passes the launcher's new projection gate: 134 first-person samples, all 32 frozen packets matching center/depth/FOV, and 6641 MiB peak working set. The bounded compute capture sees 64 programs, 54 bindings, 11 numerical camera-candidate bindings and 11 static queries (nine resolved subsets, two unresolved). Three candidate bindings have static reads overlapping view candidates and two overlap projection candidates. These are representative candidate reads, not proof of complete visibility or permission to duplicate compute writes.

La segunda ejecución, con desplazamiento sintético y auditoría compute, también supera la nueva aceptación automática: 134 muestras de primera persona, los 32 paquetes correctos en vista/profundidad/FOV y 6641 MiB máximos. Se observan 64 programas compute, 54 bindings, 11 bindings candidatos de cámara y 11 consultas estáticas: nueve subconjuntos resueltos y dos incompletos. Tres bindings tienen lecturas que coinciden con candidatos de vista y dos con candidatos de proyección. Esto no acredita visibilidad completa ni permite duplicar a ciegas sus escrituras.

Final validation for this milestone: all 171 VR tests pass after rebuilding the fixtures; the complete inherited suite passes 387 with five known environment failures and 30 skipped (230 seconds). The inherited jitter fixture now extracts the added host helpers and retains its concurrent-publication, frame-latch and observer checks. No headset session or native full-scene stereo acceptance is claimed.

Validación final de este hito: pasan las 171 pruebas VR recompiladas; la batería heredada queda en 387 correctas, cinco fallos conocidos del entorno y 30 omitidas (230 segundos). El fixture de jitter extrae ahora los helpers nuevos y conserva sus comprobaciones de concurrencia, fotograma y observación. No se acredita una sesión de visor ni estéreo nativo completo.

## Frozen first-person matrices on the GPU — English

The explicit `-FirstPersonEyeUniformProbe` diagnostic now takes both eye transforms from the frozen first-person packet. It does not add the older probe's artificial view displacement. The CPU helper validates the center view, original tracking identity/FOV and exact native depth, then builds two private copies of the complete descriptor payload. Only the audited primary prefix changes; all remaining bytes, including unreviewed current/previous aliases, stay identical. This is not a complete camera-block or scene patch.

A native SDK build, source `a2806329`, compiles without errors (84 seconds plus an 8-second host link). Its completed 150-second translation run records 126 first-person samples and passes the 32-packet projection gate. Two private 13,568-byte payloads are uploaded, copied back and compared in full after GPU timeline completion. Both destination payloads validate, and their allocations are released. Peak game working set is 6633 MiB. The original descriptors and source objects are not replaced. No draw or XR layer is submitted by this probe. Input bytes are submission-overlay copies; only the private destination readback is GPU-completed evidence.

The probe arms after verified first-person gameplay. Missing, ambiguous or expired pose ownership cannot borrow another frame; a missing packet may retry before any allocation. The recorder boundary completes earlier rendering work before the probe's transfer work. Retained raw LOCAL-space tracking poses and original times remain separate from world-space matrices.

Seven tests compile the actual probe body against controlled Vulkan stubs: legacy compatibility, frozen first-person admission, retry after a missing packet, partial allocation failure, memory-budget rejection, readback mismatch and device changes. They check upload/copy barriers, no host read or release before the fence, complete readback comparison, and release on the correct device. A separate CPU test checks exact native depth, distinct located eye views, untouched unknown bytes and atomic rejection. All 179 VR cases pass after rebuilding.

Local non-XR command:

```powershell
powershell -File vr/run-game.ps1 -FirstPersonReadbackTest -FirstPersonEyeUniformProbe -FirstPersonSweep Lean -SnapshotStableSaves -LoadTestSave -Seconds 150
```

The switch also enables the bounded projection-history check. Its private report must show `first_person_eye_uniform_completed` and `first_person_native_projection_audited` as true. Full camera aliases, shader coverage, scene replay, HUD composition and headset presentation remain explicitly false or pending. This validates camera data on the GPU; native eye drawing is the next gate.

## Matrices congeladas de primera persona en la GPU — Castellano

La prueba explícita `-FirstPersonEyeUniformProbe` obtiene las transformaciones de ambos ojos del paquete congelado de primera persona. No añade el desplazamiento artificial de la prueba antigua. El helper valida la vista central, la identidad/FOV originales y la profundidad nativa, y crea dos copias privadas del payload completo. Solo cambia el prefijo principal auditado; los demás bytes y aliases actuales/anteriores quedan idénticos. No es un parche completo del bloque de cámara ni de la escena.

La build SDK `a2806329` compila sin fallos: 84 segundos y 8 para enlazar el host. Una prueba completa de desplazamiento de 150 segundos registra 126 muestras y supera la comprobación de 32 paquetes. Se suben dos payloads privados de 13.568 bytes, se leen de vuelta tras completar la GPU y se comparan enteros. Las dos copias coinciden y sus asignaciones se liberan. El máximo del proceso es 6633 MiB. No se sustituyen descriptores ni objetos originales, y esta prueba no dibuja ni envía capas XR. La entrada procede del overlay de envío; la lectura GPU completada corresponde a los destinos privados.

Se activa después de detectar primera persona en juego. Una pose ausente, ambigua o caducada no puede usar otro fotograma; puede reintentarse antes de asignar recursos. La frontera del recorder completa el trabajo de render anterior antes de las transferencias. Las poses LOCAL originales y sus tiempos permanecen separadas de las matrices del mundo.

Siete pruebas compilan el cuerpo real con stubs Vulkan controlados: compatibilidad anterior, admisión de primera persona, reintento sin paquete, fallo parcial de asignación, límite de memoria, lectura incorrecta y cambio de dispositivo. Comprueban barreras, ausencia de lectura/liberación antes del fence y liberación en el dispositivo correcto. Otra prueba CPU verifica profundidad exacta, vistas distintas, bytes desconocidos intactos y errores sin salida parcial. Pasan los 179 casos VR recompilados.

El comando anterior realiza la prueba local sin visor. Activa también el historial acotado y exige ambos indicadores privados de aceptación. Siguen pendientes aliases completos, cobertura de shaders, replay de escena, HUD y presentación en las Quest. El próximo paso es comprobar un dibujo nativo por ojo.

The final inherited run for the GPU payload milestone passes 387 tests with the same five known environment failures and 30 skipped (239 seconds), including recorder synchronization coverage. No new headset session has been opened.

La batería heredada final de este hito repite 387 correctas, los cinco fallos conocidos del entorno y 30 omitidas (239 segundos), incluida la cobertura de sincronización del recorder. No se ha abierto otra sesión de visor.
