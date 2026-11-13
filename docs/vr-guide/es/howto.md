# Cómo compilar y probar

[Inicio](index.md) · [Cómo empezar](howto.md) · [Estado](status.md) · [Desarrollo](development.md) · [Ayuda](help.md) · [English / Castellano](../en/howto.md)

## Requisitos

Windows es la plataforma de desarrollo probada. Quest 3 utiliza Virtual Desktop y su runtime OpenXR, VDXR. Mantén la instalación funcional de Hookshot separada del repositorio. No sustituyas su ejecutable, renderer, partidas ni configuración.

Para las herramientas VR sin juego: Git, CMake 3.24+, Ninja y compilador C/C++17. El script puede leer llvm-mingw de Hookshot Builder o aceptar `-ToolchainRoot`. Descarga el SDK oficial OpenXR fijado en `work/`, excluido de Git.

```powershell
git clone --branch vr/bootstrap https://github.com/yeladies/HookshotVR.git
cd HookshotVR
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build.ps1
ctest --test-dir work/vr-build --output-on-failure
```

Esto compila herramientas y ejecuta pruebas sin juego; no crea una compilación jugable. Con el visor conectado, comprueba capacidades:

```powershell
.\workr-build\hookshot_vr_probe.exe
```

Un resultado correcto solo confirma capacidades anunciadas, no estereoscopía, seguimiento ni rendimiento real. La escena sintética independiente sí abre una sesión en el visor:

```powershell
.\workr-build\hookshot_vr_scene.exe --seconds 20
```

## Compilar el juego aislado

Necesitas tu instalación local compatible y las herramientas de Builder. El identificador revisado del juego es `e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348`, ABI del renderer 19; consulta `vr/upstream.json`. Revisa los parámetros de [la guía técnica](../../vr-game-build.md) y los scripts para tus rutas. Empieza con el plan:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -PlanOnly
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -Jobs 2
```

La compilación completa necesita bastante disco y RAM. Limita los procesos de compilación en equipos modestos. El código traducido, los binarios, cachés y datos del juego permanecen privados en `work/`. No utilices `-SkipLift` si cambian las entradas de traducción o los hooks del juego.

## Comprobar movimiento sin visor

Con el juego y renderer aislados compatibles y una partida local de prueba preparada:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-moving-preview.ps1 -Offscreen -Seconds 15
```

Inicia el productor privado y el observador GPU sin abrir XR. La preparación puede necesitar unos 90 segundos antes de la prueba medida. Comprueba cambios de imagen, no calidad en el visor. Los recibos de compilación deben coincidir; no desactives el rechazo de binarios obsoletos.

## Probar el movimiento en Quest

Conecta Virtual Desktop, verifica que el visor está disponible y ejecuta:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-moving-preview.ps1 -Seconds 60
```

El productor carga una copia privada de la partida y el visor muestra la escena durante 60 segundos tras la preparación. Ambos procesos tienen límites y limpieza. La cámara de origen sigue en tercera persona. Comprueba color, estereoscopía, orientación, escala y movimientos pequeños de cabeza. Informa de huecos, imágenes obsoletas o malestar. Este modo no integra controles VR ni audio verificado.

Para calibración congelada, sigue primero [captura y preparación de malla](../../vr-frozen-depth-preview.md) y ejecuta `vr/run-depth-preview.ps1 -Seconds 60`. Las carpetas `work/headset-preview` y `work/headset-live-preview` preparadas en el PC de desarrollo son privadas; no están disponibles para descargar del repositorio.

Para la nueva prueba de cámara del juego real, consulta [el prototipo inmersivo monoscópico](../../vr-immersive-game-preview.md). Utiliza ABI19 y es independiente del visor de profundidad.
