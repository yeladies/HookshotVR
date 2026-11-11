# VR bootstrap validation

Date: 2026-11-11. Source baseline: `940eb9ef34134913d2bc2a7b3ed5cb3caea58ef3`.

## New VR code

- Windows x64 build: passed with llvm-mingw clang 23.1.5 and CMake/Ninja.
- Official Khronos OpenXR-SDK 1.1.65 pinned by commit; static loader built locally.
- Native contract test: passed, including ten groups of capability/error/cleanup/output checks.
- Standalone launch initially exposed a dependency on compiler runtime DLLs; the MinGW targets
  now link statically and run from a plain terminal without a compiler PATH.
- Live runtime, visor disconnected: instance creation and extension discovery worked;
  `xrGetSystem` returned `XR_ERROR_FORM_FACTOR_UNAVAILABLE`, reported as exit 3.
- Live runtime, visor connected through Virtual Desktop: exit 0, runtime `VirtualDesktopXR`,
  system `Meta Quest 3`, `XR_KHR_vulkan_enable2` present, orientation and position tracking
  advertised, two primary-stereo views at recommended 2112 x 2304, one sample each.

The recommended view size is not a measured performance target. No actual pose samples,
session, controller actions, Vulkan device, rendered frames or game integration were tested.
Linux/MSVC builds are configured for CI but have not yet been observed passing.

## Inherited suite

`python -I -B tests/run_all.py --jobs 2` completed: **151 passed, 58 failed, 211 skipped**.
This ran against unchanged upstream game/runtime/renderer code in the same checkout.
The environment does not provide the compiler names expected by many inherited tests;
examples fail trying to invoke `g++`, while others are skipped by tool/header discovery.
Other observed failures include .NET restore/run fixtures and Windows symlink privilege errors.
Not every failure has been classified. These results do not establish a clean upstream baseline.

The new VR CMake tests are independent and pass; this does not clear the inherited failures.
Keep the work on a development branch rather than describing it as a validated game build.
Before changing the renderer, prepare the upstream test toolchain and resolve or reproduce the
remaining failures against an unmodified baseline. No unrelated tests were weakened or edited.

## Installation preservation

The stable installation was not launched, updated or written by the bootstrap tools.
Its launcher, game executable, renderer, version/configuration and save-file hashes are checked
against the previously recorded local baseline. Raw logs and reports remain in ignored `work/`.
