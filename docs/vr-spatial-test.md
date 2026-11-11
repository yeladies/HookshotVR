# OpenXR/Vulkan spatial test

This executable is a standalone spatial diagnostic. It contains no game data and does not
load or change the Hookshot installation. The game is not yet playable in VR.

## Run

Build with `vr/build.ps1`, connect Quest through Virtual Desktop, put the headset on and run:

```powershell
.\work\vr-build\hookshot_vr_scene.exe --seconds 20
```

It waits up to 45 seconds for the runtime to make the scene visible, then displays it for the
requested 1..90 seconds. Exit 0 requires at least one stereo frame submitted and one valid head
pose. Exit 3 means no such rendered/tracked result; exit 2 reports an API/setup/argument error.
Controller counts must be checked separately: exit 0 alone does not establish controller tracking.

If Virtual Desktop continues to show the desktop or its menu, close the overlay and switch to
VR while the diagnostic is still running. The application respects `shouldRender`; it does not
force rendering while the runtime says the session is not visible.

## Expected scene

A dark background, a sparse floor-like grid below the initial local origin, and red/green/blue
markers about two metres in front. Left/right controller grip positions appear orange/cyan
when the relevant action is active and its position and orientation are valid. Each marker is
a camera-facing rectangle projected from a 3D point independently for each eye; this is not a
triangle/shader/depth-buffer test. Marker overlap uses a simple far-to-near painter order.

Move the head gently sideways, forward/back and through roll. Stationary markers should remain
anchored in space. Move each controller into view. Subjective appearance and comfort require
user observation; successful submission counters alone do not prove what the headset displayed.

The output JSON includes valid sample counts, last poses in metres/quaternions, and the range of
head positions. Treat these as local diagnostics, not assets to publish. Vulkan is selected through
`XR_KHR_vulkan_enable2`, using the runtime-selected GPU and a Vulkan 1.3 device. The test does not
enable or validate the full mesh-shader and memory-import feature set that Hookshot requires.

## Implementation limits

- Two independent runtime-owned swapchains, capped at 1200 x 1300 per eye for this lightweight test.
- One queue and synchronous per-eye GPU fence waits; no optimization or game-performance claim.
- LOCAL reference space, not a calibrated floor, body controller or player origin.
- Grip poses through the Oculus Touch interaction profile; no aim action, buttons, haptics or hand IK yet.
- Validity flags gate poses; this is not a guarantee that a pose was freshly optically tracked.
- Session READY/STOPPING/EXITING/loss events and ordinary no-render frames are handled. Exhaustive
  device-loss/disconnect/recenter fault injection and formal Vulkan validation remain to be done.
- Vulkan-Headers 1.3.280 are fetched at commit `577baa05033cf1d9236b3d078ca4b3269ed87a2b`.
  `VULKAN_HEADERS_SOURCE_DIR` optionally supplies a local header checkout. No Vulkan import library
  or global Vulkan SDK installation is required; functions are loaded from the system loader.

Game-free tests check stereo disparity, distance scaling, head roll, framebuffer orientation,
asymmetric FOV, edge clipping, out-of-view/near/invalid input and full-pose validity requirements.

The next integration step is to validate live output and then build a separate flat game baseline,
before routing actual scene geometry to the two eyes. This diagnostic is not a drop-in renderer DLL.
