# Architecture, builds and upstream updates

[Home](index.md) · [How-to](howto.md) · [Status](status.md) · [Development](development.md) · [Help](help.md) · [English / Castellano](../es/development.md)

## Source map

| Location | Purpose |
|---|---|
| `vr/src/` | OpenXR tools, viewer, pose math and packet validation |
| `vr/shaders/` | Original viewer shader sources |
| `gpu/vk/vk_xr*`, `gpu/vk/xr_*` | Opt-in renderer integration and bounded diagnostics |
| `vr/tests/` | Game-free contracts, parsers and update tests |
| `vr/upstream.json` | Reviewed source base, game build profile, ABI and integration files |
| `docs/vr-*.md` | Detailed engineering notes and validation records |
| `work/` | Ignored private builds and data; never publish |

The live producer captures matched color, depth and camera matrices from the same frame. It atomically publishes a bounded packet. The viewer validates it, reuses a GPU storage buffer and reconstructs the visible surface for independent eye poses/FOVs. Invalid or stale frames are rejected. This viewer is separate from the native full-scene stereo investigation.

Read [live transport](../../vr-live-depth-preview.md), [frozen reconstruction](../../vr-frozen-depth-preview.md), [camera integration](../../vr-camera-integration.md) and the [checkpoint](../../vr-development-checkpoint.md). Renderer host-facing table changes require an ABI bump; generated shader headers must be regenerated from their sources.

## Updating Hookshot

The mod is a source adaptation, not a universal drop-in DLL. The recorded base is `940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`. A clean reapplication does not establish compatibility with newer upstream changes or game revisions.

From a clean committed checkout, add the upstream remote once if absent, fetch and choose the exact revision to review:

```powershell
git remote add upstream https://github.com/NotRexed/Hookshot.git
git fetch upstream
python -I -B vr/update_upstream.py audit --revision upstream/main
python -I -B vr/update_upstream.py prepare --revision upstream/main
```

Use an explicit commit or tag instead of `upstream/main` when pinning an evaluation. `prepare` creates an isolated worktree under `work/upstream-updates/`, retains conflicts and leaves compatibility **unverified** and `release_ready=false`. It never installs a candidate. Review integration changes, rebuild privately with fresh translation when needed, run source/VR tests, verify flat gameplay, then verify in the headset before promoting the recorded profile. Preserve a known working commit for rollback.

## Checks

```powershell
ctest --test-dir work/vr-build --output-on-failure
python -I -B vr/tests/test_update_upstream.py
python -I -B tests/run_all.py --jobs 2
```

The inherited suite needs its documented compiler and fixture dependencies. Record failures honestly. GPU-only synthetic motion checks are explicit local checks, not portable CI requirements. Headset tests require a person available to inspect the result.

For the complete update rules see [upstream compatibility](../../vr-upstream-updates.md). These bilingual guide pages are part of the allowed source-only patch and travel with update candidates.
