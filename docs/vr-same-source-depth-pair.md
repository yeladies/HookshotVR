# Same-source GPU depth pair

## English

An authored compute producer now reconstructs two eye images from **one immutable color/D32 packet**, then copies the complete pair to an offscreen GPU target. It accepts a rigid source-camera-from-eye transform and each eye's tangent-space frustum. This is depth reconstruction of one captured view, not native per-eye rendering of all game geometry.

The pair contract supplies one source object to both eyes and verifies source dimensions, format, projection/optical center, origin, generations, recording order, shared target pose, queue family and disjoint output/metadata/counter ranges. An older immutable source can be reused with a newer pose when its queue ordering and lifetime are proved by the owner. Identity tags and declarations are contracts, not independent proof of correct scene production.

The GPU uses bounded fixed-point depth lookup and rejects invalid depth, uncovered rays or excessive reprojection residual. Missing information writes black; the shader cannot invent disoccluded surfaces or content beyond the source FOV. Both supported source formats are the existing native packed color formats (122/123). Output currently clamps decoded colors to 0–1; final game tonemapping and HUD composition are not implemented here.

The diagnostic counters can be disabled for the production path. The caller must still prove the live allocation extents, source association, immutable metadata, synchronization and lifetime. This component has not been connected to the native game producer or OpenXR session.

### Actual RTX 2060 SUPER checks

The new explicit GPU test checks all 32,768 target pixels across four completed submissions, with reused allocations:

- Both eyes reconstruct an overscanned plane with analytically known parallax.
- A changed source pattern and the second packed color format detect stale data.
- A rotated/translated eye transform agrees with an independent CPU ray/plane oracle.
- Nonfinite depth rejects every affected pixel without modifying the source packet.

Both eye images then travel through the existing same-state GPU copy component. There is no CPU pixel transfer between reconstruction and pair composition. The authored test input is initially uploaded by the CPU, and completed output is read back for verification; this is not a live-game GPU transport measurement. No game data, XR runtime, window or display change is involved.

The shared standalone Vulkan test context now explicitly enables the shader-int64 feature used by physical-address arithmetic. Existing GPU root positive/rejection checks and same-state image-copy checks still pass after extracting this context. All 106 game-free VR CTest cases pass. The authored shader header is generated from the adjacent source; no game shader is used.

```powershell
work/vr-build/hookshot_vr_depth_pair_smoke.exe --run
```

Next: inspect a private real color/depth packet through this producer, then connect a bounded immutable GPU source owner to the game and OpenXR. Actual headset image quality, performance, disocclusions, camera mapping/scale, full FOV and separate UI still require integration and testing. The existing playable temporal preview is unchanged. No headset test is requested while the user is unavailable.

## Castellano

El nuevo productor de cómputo reconstruye los dos ojos desde un único paquete inmutable de color y profundidad y copia la pareja completa a un destino GPU. Es reconstrucción desde una captura, no renderizado nativo de toda la geometría por ojo.

El contrato usa un solo origen para ambos ojos. Comprueba dimensiones, formato, proyección, centro óptico, origen, generaciones, orden de grabación, pose común de destino, familia de cola y rangos de memoria separados. Puede reutilizar una captura anterior con una pose nueva si el propietario demuestra sincronización y duración. Las etiquetas no sustituyen esa prueba.

El shader rechaza profundidad inválida, rayos sin cobertura y discrepancias excesivas; no inventa superficies que la captura no contiene. Los huecos se muestran negros. Se admiten los dos formatos de color nativos existentes; el tratamiento final de color y el HUD aún no están conectados. Los contadores de diagnóstico se pueden desactivar.

La RTX 2060 SUPER ha comprobado 32.768 píxeles en cuatro envíos con reservas reutilizadas: paralaje de un plano calculado independientemente, cambio de patrón/formato, rotación/traslación y rechazo de profundidad no finita. El paquete original queda intacto. Los dos ojos pasan después por la copia GPU conjunta, sin píxeles intermedios en CPU. El test sube inicialmente datos creados por nosotros y lee el resultado al finalizar para comprobarlo; no es una medición del juego en directo.

Pasan las 106 pruebas VR y las pruebas GPU anteriores tras compartir el contexto de Vulkan. No se ha iniciado el visor ni cambiado el monitor. El siguiente paso es comprobar una captura privada real y conectar un propietario GPU inmutable al juego y a OpenXR. Siguen pendientes calidad, rendimiento, huecos, escala, FOV y UI real. La prueba temporal jugable permanece como estaba.


### Private actual-scene verification

A frozen private game packet was processed at 1152×648 per eye. The GPU accepted 741,378 / 741,165 of 746,496 pixels (99.314% / 99.286%). Coverage rejected 2,993 / 3,231 pixels, and reprojection residual rejected 2,125 / 2,100; no invalid-depth pixel was reported for this packet. The entire source packet stayed bit-exact. The two images differ at 587,171 pixels. These are acceptance/transport observations, not proof of physically correct reconstruction at every pixel or headset quality.

The local output was visually inspected: the complete scene is present, with missing pixels concentrated around silhouettes/depth discontinuities. A single source cannot reveal disoccluded surfaces; transparent effects can also differ from the opaque depth that accompanies them. Scale/IPD are uncalibrated. The test initially uploads a frozen file and reads completed output for review. It is still neither a live-game GPU source nor an OpenXR submission.

The offscreen fixture now supports real source dimensions while retaining the four-submission analytic oracle. The capture tool refuses paths outside project/work and existing output files, and writes only a fresh private PPM. No game pixels, input packet, PPM or PNG is published.

```powershell
work/vr-build/hookshot_vr_depth_pair_capture.exe --run --capture work/<private-frame>.hsvrd --output work/validation/<fresh-name>.ppm
```

Castellano: una captura real congelada produjo 1152×648 píxeles por ojo. Se aceptan 741.378 / 741.165 de 746.496 (99,314% / 99,286%); no es una medición de exactitud física. Hay 2.993 / 3.231 píxeles sin cobertura y 2.125 / 2.100 rechazados por discrepancia. Fuente completa intacta y 587.171 píxeles diferentes entre ojos. La revisión visual muestra la escena completa, con huecos en siluetas/cambios de profundidad. No hay datos para superficies ocultas y la transparencia puede no compartir la profundidad opaca. Escala sin calibrar. Es lectura de archivo congelado y salida local, sin juego en directo ni OpenXR. Todos los datos e imágenes siguen privados.


Current integration: the [native live-GPU diagnostic](vr-live-gpu-depth-pair.md) now reconstructs both eyes without downloading/uploading its source. The frozen-file checks above remain independent diagnostics. Castellano: el diagnóstico nativo ya consume la fuente viva sin descargarla/subirla; las pruebas de archivo anteriores siguen siendo diagnósticos separados.
