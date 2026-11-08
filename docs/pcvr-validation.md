# PCVR experiment validation

Date: 2026-11-08. Baseline: `45620e817d6000118d6077857ba2910d051de164`.
Environment: Windows x64, Python 3.12, fresh source clone. No C++ compiler found on
PATH; Vulkan/SPIR-V development headers and several optional test dependencies
were not discovered. No game build or headset run was performed.

| Check | Result |
|---|---|
| `python -I -B tests/test_openxr_preflight.py` | 8 unit tests passed; fake enumeration only |
| `python -I -B scripts/openxr_preflight.py` | Exit 1; default `System32/openxr_loader.dll` or a dependency could not be loaded |
| `git diff --check` | Passed |
| Full game-free runner, experimental tree | 43 passed, 36 failed, 28 skipped |
| Same runner, clean detached baseline worktree | 42 passed, 36 failed, 28 skipped |
| Comparison of failed check names | Identical sets; no newly failing check |

The full-run totals count test scripts/checks, not individual unit test methods.
The experimental run included the new probe test script; its final eight-method
version was also executed directly after adding three further error-boundary cases.

Reproduce the general comparison with a clean baseline worktree and a writable
temporary folder. For each tree, from its root:

```powershell
New-Item -ItemType Directory -Force work/pcvr-tmp | Out-Null
$env:TEMP = (Resolve-Path work/pcvr-tmp).Path
$env:TMP = $env:TEMP
python -X utf8 -I -B tests/run_all.py --jobs 4 --timeout 60
```

The initial default temporary directory was not writable for several checks.
Redirecting TEMP/TMP improved execution. UTF-8 output was needed to keep the
baseline runner's failure reporting from crashing on a replacement character.
Remaining failures include missing compiler executables, Windows link-fixture
permissions, and a `Path.read_text(newline=...)` argument unsupported by Python
3.12. The baseline comparison establishes these are reproducible without the XR
addition; it does not make the whole suite green or prove all failures harmless.
No unrelated test or runtime behavior was changed to hide those failures.

The direct probe result establishes only a failure to load the default DLL in this
environment. No alternate DLL was supplied; the installed runtime inventory was
not established. There is **no evidence here** of working OpenXR initialization,
Vulkan sharing, XR swapchains, tracking, controllers, stereo or headset display.

The implementation deliberately stops at extension enumeration. An in-game
session prototype needs a real SDK/compiler and runtime validation before changing
the renderer's device creation or shared queue scheduling. The next visible
prototype is specified in [pcvr-experiment.md](pcvr-experiment.md).
