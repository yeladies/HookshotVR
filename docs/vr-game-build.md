# Isolated flat-game baseline

The next prerequisite is an ordinary flat build of the same source we will modify for VR.
Do not mix this renderer with the installed v2.1.0 executable.

`vr/build-game.ps1` reads existing Builder tools and the stable game's `.hookshot_exefs` and
`romfs` folders. It invokes the source translation and native compile directly, rather than
running the installer/update pipeline. Generated code, objects and binaries stay under ignored
`work/game-build`. This avoids duplicating roughly 26 GB of RomFS on this PC.

```powershell
# Check paths, supported build header, dependencies and disk before writing build outputs:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -PlanOnly

# Translate and build with two compiler jobs:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1

# Resume compilation only after a completed translation from the current source:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/build-game.ps1 -SkipLift
```

The script does not download tools, launch the game, write to the Builder installation, copy
RomFS, or update the stable game. `-SkipLift` is a manual resume option, not automatic validation
that old generated code still matches later source changes. Do a fresh lift after pipeline/hook changes.
Disk checks are conservative gates, not a prediction of peak use; monitor free space during a build.

`vr/run-game.ps1` prepares `work/game-run` with the matching local executable/renderer and required
local inputs, private saves/options/caches, an explicit read-only RomFS path and update checks off.
It defaults to a muted headless 60-second smoke run, with memory/disk guards, rather than changing
the desktop display. Use `-Window` only for a later visible test. It does not copy stable saves or
native program caches. A timeout deliberately stops only the experimental process; a smoke run
is not proof of gameplay or save compatibility. No game-derived output is published.

The build copies the Builder's hot-function list, link ordering, play profile and optional icon
resource into the private work folder. `A64_HOT_FUNCS`, hot-chunk emission, `split_hot.py` and the
local PGO profile are used as in the installer pipeline. `-LiftOnly` generates and partitions
source without compiling. These local inputs and generated files must not be published.

Once the flat baseline has been validated, connect the OpenXR session/device work to Hookshot's
own Vulkan device creation. Preserve the source's submit-time captures and synchronization.
Physical mechanics follow later; the saved [reference projects](vr-reference-projects.md) are
part of that research, not dependencies of the baseline build.

## Observed result and shader startup

The native build and an isolated main-menu smoke passed on 2026-11-12; details and limitations are
in [validation](vr-validation.md). Full gameplay remains to be checked before treating this as a
playable baseline. This build is not yet connected to OpenXR.

The runner defaults to the renderer's supported on-demand shader mode. This avoids waiting for the
entire shader corpus before reaching the menu but can cause severe first-use stalls. Use
`-PrepareAllShaders` to request upfront preparation; the smoke timeout still applies and may expire
before preparation finishes. Stable native shader caches must not be copied when compatibility
stamps differ. Private initial options select low quality and disable fullscreen, HDR and microphone;
existing experimental options are preserved. Resource-limit termination returns 124, and an observed
nonzero early process exit returns 1. A deliberate time limit is reported separately in JSON and
always leaves `gameplay_verified` false.
