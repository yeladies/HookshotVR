# Renderer session and pose probe

`-XrSessionProbe` adds a bounded, zero-layer session to the real renderer's negotiated Vulkan device.
It does not display game images in the headset, replace the game camera, create eye swapchains or
repeat any game simulation/graphics replay. Default startup remains flat with XR disabled.

```powershell
# Uses the SDK-enabled renderer and official loader prepared in vr-renderer-openxr.md:
powershell -NoProfile -ExecutionPolicy Bypass -File vr/run-game.ps1 -Seconds 35 -XrSessionProbe
```

The switch implies `-XrNegotiation`. The renderer's `RX_VK_XR_SESSION_PROBE=1` option starts an
approximately ten-second diagnostic, which closes itself while the game can continue in flat mode.
The existing wrapper bounds the whole game run and requires a session-close summary with nonzero
valid head and stereo-view pose counts and no reported XR API errors. This checks live pose access,
not visible game stereo or physical tracking accuracy. No changes to the host/renderer ABI were made.

## Ownership and synchronization

One renderer-owned thread creates the session/reference spaces, polls session events, waits/begins/
ends frames, locates poses and destroys its session resources. READY begins PRIMARY_STEREO;
STOPPING ends it; EXITING and loss events terminate the probe. Only OPAQUE PRIMARY_STEREO with two
views is accepted for this initial implementation. LOCAL and VIEW reference spaces supply eye and
head poses at the same predicted display time. The probe always ends a successfully begun frame,
even when locating a pose fails. It submits zero composition layers, including when shouldRender
is true. There are no application Vulkan submissions or swapchain acquire/release calls here.

OpenXR may nevertheless access the bound Vulkan queue in xrBeginFrame and xrEndFrame. Those calls
use the renderer's existing `dev.present_mu`, also used for access to its graphics queue. xrWaitFrame
runs outside that lock and outside replay locks. This follows the official
[Vulkan concurrency requirements](https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_KHR_vulkan_enable-concurrency.html).
The first live attempt was interrupted during compilation to add this synchronization before any
session probe launched; tests now verify lock ownership at begin/end and its absence during wait.

Explicit reset and the existing window-close notification request stop and join the owner before
unloading the runtime. Startup waits up to ten seconds; stop waits up to five seconds. A runtime
call itself can block longer: if the owner fails to stop, the adapter retains handles/library code
rather than unloading resources under a live thread. The outer smoke wrapper remains the process
watchdog. Session close polls for STOPPING briefly after requesting exit, then destroys spaces and
session. Native fixtures cover normal/partial cleanup and frame balancing, but do not establish
correct behavior for every driver/device-loss or hostile-runtime stall.

The small bridge owner has process lifetime; it is deliberately not joined from a DLL's CRT
destructor under Windows loader lock. The host retains the renderer DLL. A future reusable renderer
shutdown/device-recreation API must explicitly preserve that ownership rule; this is not a complete
engine teardown design. The diagnostic's automatic stop avoids leaving a persistent XR thread during
ordinary continued flat play. A joined/finished owner can be restarted explicitly in internal tests.

## Pose mailbox

`xr_vulkan_latest_frame` copies an internal C++ snapshot under a mutex. It contains frame sequence,
predicted XR display time, observation time on the monotonic host clock, origin generation, running/
focus/shouldRender state, head pose, both eye poses, and each runtime-provided asymmetric FOV.
Validity/tracked flags accompany the poses. A consumer must check them and must not treat orientation
alone as positional tracking. The runtime's eye offsets are already included.

A new session advances the origin generation. LOCAL reference-space-change notifications advance it
again and suppress pose validity until the event's change time. Stop and terminal paths clear the
mailbox. Reads fail with a cleared output if the owner has stopped or a sample is older than 250 ms.
That age limit is a diagnostic stale-data guard, not a target latency or a prediction algorithm.
No game camera consumer uses this mailbox yet. Frame sample production on this thread must not be
mistaken for assigning the same frame identity to independently running game simulation/rendering.

## Validation and next work

Thirty-one native CTest cases pass locally. The final live run completed 697 zero-layer frames with
697 valid head and stereo-view samples and closed its session without reported XR API errors. New synthetic cases exercise focus/session/instance loss,
invalid poses, shouldRender false, missing functions, partial startup, wait/begin/end/locate failures,
origin invalidation, sample expiry during a blocked wait, balanced zero-layer frames, queue locking,
resource cleanup, automatic duration and restart with a new origin generation. They use an authored
fake loader and no runtime, GPU or game inputs. Real-run results are in [validation](vr-validation.md).

Next connect an immutable main-game camera snapshot and a correctly timed consumer. Stereo image
creation still needs per-eye projection/depth/Y conventions, camera-dependent replay isolation,
independent temporal history, culling and UI handling. Head/body collision and physical controller
mechanics remain later milestones. Do not duplicate write-producing compute or simulation to get
a second eye, or label a duplicated flat image as native stereo.