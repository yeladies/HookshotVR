// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_scene_camera.h"
#include <cstdlib>
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
bool xr_scene_capture_enabled() {
    static const bool enabled=[] {const char* e=std::getenv("HOOKSHOT_VR_SCENE_CAPTURE");return e&&e[0]=='1';}();
    return enabled;
}
void xr_scene_camera_observation(const SceneCameraEvidence& draw,const float* block,size_t count) {
    if(!xr_scene_capture_enabled())return;
    std::lock_guard lock(capture_mutex);
    CameraCapturePair pair{};const bool fresh=capture.latest(now_ns(),pair);
    const auto result=match_scene_camera(block,count,draw,fresh?&pair:nullptr);
    static uint64_t totals[5]{},observed=0,matched_frames=0,last_match_frame=UINT64_MAX;
    ++totals[static_cast<unsigned>(result)];++observed;
    if(result==SceneCameraMatch::Match&&draw.replay_frame!=last_match_frame){++matched_frames;last_match_frame=draw.replay_frame;}
    // Aggregate evidence only. A matching candidate is not unique camera ownership or permission to override it.
    if(observed==1||observed%2000==0)logf("vr-scene: draw candidates %llu, invalid %llu, no fresh camera %llu, view mismatch %llu, projection mismatch %llu, matched %llu, matched replay frames %llu",
        (unsigned long long)observed,(unsigned long long)totals[0],(unsigned long long)totals[1],(unsigned long long)totals[2],
        (unsigned long long)totals[3],(unsigned long long)totals[4],(unsigned long long)matched_frames);
}}