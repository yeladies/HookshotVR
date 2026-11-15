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
