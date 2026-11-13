# How to build and try the previews

[Home](index.md) · [How-to](howto.md) · [Status](status.md) · [Development](development.md) · [Help](help.md) · [English / Castellano](../es/howto.md)

## Requirements

Windows is the tested development platform. Quest 3 uses Virtual Desktop and its OpenXR runtime, VDXR. Keep your working Hookshot installation separate from the source checkout. Do not replace its executable, renderer, saves or settings.

For the game-free VR tools: Git, CMake 3.24+, Ninja and a C/C++17 compiler. The build helper can read an existing Hookshot Builder llvm-mingw toolchain or accept `-ToolchainRoot`. It downloads the pinned official OpenXR SDK into ignored `work/`.

```powershell
git clone --branch vr/bootstrap https://github.com/yeladies/HookshotVR.git
cd HookshotVR
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build.ps1
ctest --test-dir work/vr-build --output-on-failure
```

This builds the tools and runs game-free tests. It does not create a working game build. With the headset connected, the capability probe is:

```powershell
.\workr-build\hookshot_vr_probe.exe
```

The probe checks advertised capabilities; success does not establish working stereo, tracking or performance. The standalone synthetic scene opens a headset session:

```powershell
.\workr-build\hookshot_vr_scene.exe --seconds 20
```

## Build the isolated game

Use your own supported local installation and Builder inputs. The reviewed game build is `e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348`, renderer ABI 19; see `vr/upstream.json`. Check the parameters in [the isolated build guide](../../vr-game-build.md) and the scripts for your paths. Plan first:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -PlanOnly
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -Jobs 2
```

A full build requires substantial disk space and RAM. Limit compiler jobs on modest PCs. Keep translated code, binaries, caches and all game inputs private under `work/`. Do not use `-SkipLift` after changing translation inputs or game hooks.

## Check movement without a headset

After a matching isolated game/renderer build and compatible local test save are prepared:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-moving-preview.ps1 -Offscreen -Seconds 15
```

This launches the private producer and GPU observer without opening XR. It may need roughly 90 seconds before the measured preview starts. Its output proves image changes, not headset quality. Build receipts must match; never bypass a stale-build rejection.

## Try the moving preview in Quest

Connect Virtual Desktop, verify the headset is available, then run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-moving-preview.ps1 -Seconds 60
```

The producer loads a private save snapshot and the viewer runs for 60 seconds after preparation. Both processes are bounded and cleaned up. Source camera position remains third person. Inspect color, stereo, orientation, scale and small head movements. Report holes, stale frames and discomfort. This mode has no integrated VR controls or verified audio.

For frozen calibration, first follow [capture and mesh preparation](../../vr-frozen-depth-preview.md), then use `vr/run-depth-preview.ps1 -Seconds 60`. The prepared `work/headset-preview` and `work/headset-live-preview` folders on the development PC are private local artifacts, not downloads available from this repository.

For the new real-game head-camera test, see [the monoscopic immersive prototype](../../vr-immersive-game-preview.md). It uses ABI19 and is separate from the depth viewers.
