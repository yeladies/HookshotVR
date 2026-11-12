// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_capture.h"
#include <chrono>
#include <mutex>
namespace mkwvk {
void logf(const char*,...);
namespace {
std::mutex capture_mutex;
CameraCapture capture;
int64_t now_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
void xr_camera_observation(const MkwVkCameraObservation* sample) {
    if(!sample)return;
    std::lock_guard lock(capture_mutex);
    capture.observe(*sample,now_ns());
    const auto s=capture.stats();
    const auto total=s.primary+s.uniforms+s.rejected;
    if(total==1||total%300==0)logf("vr-camera: observations primary %llu, uniforms %llu, matrix matches %llu, rejected %llu",
        (unsigned long long)s.primary,(unsigned long long)s.uniforms,(unsigned long long)s.matches,(unsigned long long)s.rejected);
}
bool xr_camera_latest(CameraCapturePair* out) {
    if(!out)return false;
    std::lock_guard lock(capture_mutex);return capture.latest(now_ns(),*out);
}
}