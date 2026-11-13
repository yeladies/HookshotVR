# Troubleshooting, reports and contribution

[Home](index.md) · [How-to](howto.md) · [Status](status.md) · [Development](development.md) · [Help](help.md) · [English / Castellano](../es/help.md)

## Common problems

| Symptom | What to check |
|---|---|
| No headset / OpenXR error | Virtual Desktop connection and the configured VDXR runtime. A connected desktop alone does not prove an available OpenXR session. |
| Only a cinema screen | You launched the flat-quad test. Use the depth preview guide for reconstructed surfaces; native game stereo is still in development. |
| No sound | Audio routing/integration is not verified. Do not interpret a silent preview as a completed audio implementation. |
| Holes or missing surfaces | A single depth view cannot reveal hidden geometry; inspect small head translations first. |
| Jerky scene updates | Last offscreen capture cadence was about 4.5/s; this is independent of the viewer's headset frame loop. Headset FPS has not been measured for this mode. |
| Stale/mismatched receipt | Rebuild the matching isolated components. Never copy random DLLs into the stable installation. |
| Viewer stops showing the scene | Check producer exit/timeout and packet freshness; stale input is deliberately suppressed. |
| Monitor loses signal | Stop the test and restore the display connection before another attempt. Record the exact symptom; no-signal is different from a Windows blue-screen error. |

## Useful test report

Record commit, Windows/GPU/driver, runtime, headset, preview command, duration and observed behavior. Say which mode you tested. For depth tests describe both eyes, scale, color, orientation, small head translations, motion cadence, closure and monitor stability. Do not attach game-derived packets, geometry, binaries, saves or private configuration. Review any text log before posting to remove personal paths or identifiers.

Use the [project issues](https://github.com/yeladies/HookshotVR/issues) for mod-specific findings. Upstream Hookshot problems should first be reproduced without this experimental mod. Reports in English or Spanish are welcome for this project.

## Publication and contribution

Keep contributions focused, with source tests and honest validation. Read [contribution rules](../../../CONTRIBUTING.md) and [agent rules](../../../AGENTS.md). The inherited code and its licenses remain intact; this is an independent experimental adaptation, not an official upstream VR release.

Publish original mod source, authored shaders, tests and documentation. **Never upload game files, keys, generated game code, game shader dumps, captures, reconstructed meshes, saves, private settings, `work/`, or local executable/DLL packages.** “Upload everything” means all publishable project work, not private game data.

There is no ready-to-play release, guaranteed update compatibility, native full-game 6DoF or physical weapon implementation yet. No donation or hardware purchase is required to follow the project.
