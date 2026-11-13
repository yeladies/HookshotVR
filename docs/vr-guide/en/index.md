# HookshotVR — project guide

[Home](index.md) · [How-to](howto.md) · [Status](status.md) · [Development](development.md) · [Help](help.md) · [English / Castellano](../es/index.md)

Experimental PCVR research for **Hookshot**, targeting Quest 3 on Windows through Virtual Desktop / VDXR. The goal is native stereo, head tracking with six degrees of freedom, and eventually tracked interaction. **This is not a finished or playable native VR mod.**

## Start here

- [Build and run](howto.md): requirements, isolated builds and bounded previews.
- [Status and roadmap](status.md): what works, measurements and remaining work.
- [Architecture and upstream updates](development.md): source layout, validation and rebasing.
- [Troubleshooting and contributing](help.md): useful reports and publication rules.

## What is available?

| Mode | Current evidence |
|---|---|
| Flat cinema screen | Image and world-fixed screen confirmed in Quest; audio unresolved. |
| Frozen depth reconstruction | Independent eye projections and head poses implemented; offscreen GPU check passed; headset perception pending. |
| Moving depth reconstruction | Game supplies successive color/depth frames; offscreen motion verified; headset check pending. |
| Native full-scene stereo | Research in progress; an unchanged isolated draw was reproduced, not a complete stereo scene. |

The depth modes reconstruct visible surfaces from one game camera. They cannot recover hidden geometry and are not equivalent to native rendering. The source camera is still third person. VR gameplay controls, physical weapons and audio routing remain unfinished.

Only source, authored shaders, tests and documentation are distributed here. There is no public game build or ready-to-play VR download. You need your own supported game data locally.

[Development branch](https://github.com/yeladies/HookshotVR/tree/vr/bootstrap) · [Original Hookshot](https://github.com/NotRexed/Hookshot) · [Technical checkpoint](../../vr-development-checkpoint.md)
