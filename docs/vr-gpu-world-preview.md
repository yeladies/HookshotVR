# GPU world preview / Prueba GPU del mundo

## English

This opt-in alpha connects the continuous immutable GPU eye stream to a real OpenXR projection layer. Both eyes come from **one source scene state and depth image**, with different tracked eye poses. It replaces nearby-frame temporal pairing and CPU pixel readback/upload in this mode. It is depth reconstruction, not full geometry rendered twice.

Run from the project root after building the SDK-enabled experimental renderer and host:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-gpu-world-preview.ps1
```

Connect Quest 3 through Virtual Desktop first. The bounded test lasts 180 seconds and loads a private save copy using the existing scripted input. The world source is armed after about 80 seconds; the headset can remain black during loading. Start with small head turns once the world appears. The original installation and its saves are read-only. Audio is enabled. The guarded wrapper prevents concurrent Hookshot runs and keeps console selection away from the child watchdog.

The default `Clarity` profile requests docked 1920x1080 output, 1080x1080 per eye, a 25% source tangent margin and a separate 10% rendered-eye guard band. The actual internal world resolution is measured in the private immersive report; output resolution does not prove source detail. `-Profile Baseline` retains the 720p request, 1024x1024 eyes and no eye guard band for comparison. Both profiles now use depth-aware bilinear color sampling, restricted to neighbours within 1% relative depth. This does not promise higher source detail or full coverage on fast turns. Color uses raw clamped linear RGBA8 UNORM; final game tone mapping is still pending. The runtime must offer that format: unsupported runtimes fail explicitly. No automatic untested color fallback is used.

The consumer retains the immutable source buffer until its copy fence completes. It polls deferred copies without blocking an XR frame for seconds, reuses a valid released pair, submits its **original production poses and FOV**, rejects stale or wrong-origin pairs, and conservatively retains uncertain GPU ownership until process exit. Recenter never relabels old images as a new origin. Private JSON reports distinguish successfully ended projection frames from prepared layers and producer publications.

**Current limits:** world rendering only; menus/HUD composition, physical motion controls, first-person body calibration, full geometry stereo, transparent/disoccluded regions and final tone mapping remain pending. This is a visual diagnostic, not a playable VR release. A black loading phase or unavailable menus is not proof of a stereo transport failure.

Validation before the first real-headset run: native Windows SDK build succeeded, 134 VR tests passed (19 fake-runtime GPU ownership/presentation cases plus receipt and launch guards), and default-off isolated startup passed. The first real VDXR run completed 2,654 pair copies and 6,363 submitted projection layers with no API errors. The user saw the 3D world but reported strong pixelation, turn boundaries and dark geometry outlines. The reduced video corroborates jagged silhouette defects. The Clarity headset comparison completed with 1920x1080 world rendering, 1,399 copies and 3,568 projections. The user reported no substantial improvement. Both profiles remain visually unaccepted. See [age instrumentation and next renderer stage](vr-presentation-age-and-next-renderer.md). Reports stay under ignored `work/`; no game files, binaries or captures are published.

Please report: whether the world appears after loading, black flicker, head-turn borders, stereo comfort/depth, overall brightness and sound. Do not use menu operation as an acceptance criterion for this world-only alpha.

## Castellano

Esta alfa opcional conecta el flujo continuo de ojos GPU con una capa de proyección OpenXR real. Ambos ojos proceden de **un único estado de la escena y su imagen de profundidad**, con poses distintas para cada ojo. Este modo sustituye el emparejado temporal de fotogramas cercanos y la copia de píxeles por CPU. Es reconstrucción con profundidad; no renderiza toda la geometría dos veces.

Conecta primero las Quest 3 mediante Virtual Desktop y ejecuta el comando anterior. La prueba dura 180 segundos y carga una copia privada de la partida con las pulsaciones automatizadas existentes. La captura del mundo se activa hacia los 80 segundos; durante la carga puede no verse nada en el visor. Empieza con giros suaves cuando aparezca el mundo. La instalación original y sus partidas solo se leen. El audio está activado. El lanzador impide ejecutar dos instancias de Hookshot y separa la consola del proceso que controla el tiempo de prueba.

El perfil predeterminado `Clarity` solicita salida de 1920x1080, ojos de 1080x1080, margen del 25% en la captura y un margen distinto del 10% en las imágenes renderizadas para los ojos. La resolución interna del mundo se mide en el informe privado. `-Profile Baseline` conserva la solicitud 720p, ojos de 1024x1024 y sin margen en los ojos para comparar. Ambos perfiles incorporan filtrado bilineal de color limitado a vecinos con diferencia de profundidad relativa de hasta el 1%. Esto no garantiza más detalle en la imagen fuente ni eliminar los bordes en giros rápidos. Se requiere RGBA8 UNORM en el runtime; el mapeado de tonos final sigue pendiente.

El consumidor mantiene prestado el buffer hasta completar la copia GPU, consulta las esperas pendientes sin bloquear durante segundos y reutiliza el último par válido con sus **poses y FOV de producción originales**. Descarta imágenes caducadas o de otro origen de tracking. El recentrado no convierte una imagen antigua en una imagen del origen nuevo. Los informes privados separan pares producidos, copias terminadas y proyecciones realmente enviadas.

**Limitaciones:** solo mundo 3D; quedan pendientes menús/HUD, controles físicos, calibración del cuerpo en primera persona, estéreo de geometría completa, zonas desocultadas/transparencias y mapeado de tonos. Es una prueba visual, no una versión VR jugable completa.

Antes de la primera prueba real han pasado la compilación nativa con SDK, 134 comprobaciones VR y el arranque aislado con VR desactivado. La primera sesión VDXR terminó 2.654 copias de pares y 6.363 proyecciones sin errores de API. El usuario vio el mundo 3D, pero señaló pixelado, límites negros al girar y contornos negros en geometría; el vídeo reducido muestra contornos rotos. La comparación Clarity terminó con mundo a 1920x1080, 1.399 copias y 3.568 proyecciones; el usuario no percibió mejora sustancial. Ningún perfil está aceptado visualmente. Véase [instrumentación y siguiente etapa](vr-presentation-age-and-next-renderer.md). Comenta si aparece el mundo tras la carga, si parpadea en negro, si se ven bordes al girar, cómo notas profundidad/comodidad, brillo y sonido. Los informes permanecen en `work/`, fuera de GitHub.
