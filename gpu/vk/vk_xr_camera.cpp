// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_submission.h"
#include <cstdlib>
#include <chrono>
#include <mutex>
namespace mkwvk {
void logf(const char*,...);
namespace {
std::mutex capture_mutex;
CameraCapture capture;
SceneCameraHistory history;SceneSubmissionStore submissions;
thread_local SceneSubmissionPtr active_submission;
int64_t now_ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
}
void xr_camera_observation(const MkwVkCameraObservation* sample) {
    if(!sample)return;
    std::lock_guard lock(capture_mutex);
    const auto at=now_ns();
    const bool accepted=capture.observe(*sample,at);
    if((sample->flags&MKWVK_CAMERA_PRIMARY)&&!accepted)history.clear();
    CameraCapturePair pair{};
    if(accepted&&capture.latest(at,pair))history.observe(pair,at);
    const auto s=capture.stats();
    const auto total=s.primary+s.uniforms+s.rejected;
    if(total==1||total%300==0)logf("vr-camera: observations primary %llu, uniforms %llu, matrix matches %llu, rejected %llu",
        (unsigned long long)s.primary,(unsigned long long)s.uniforms,(unsigned long long)s.matches,(unsigned long long)s.rejected);
}
bool xr_camera_latest(CameraCapturePair* out) {
    if(!out)return false;
    std::lock_guard lock(capture_mutex);return capture.latest(now_ns(),*out);
}
SceneSubmissionPtr xr_scene_submission_capture() {
    if(!xr_scene_capture_enabled())return {};
    std::lock_guard lock(capture_mutex);
    return std::make_shared<const SceneSubmissionSnapshot>(history.snapshot(now_ns()));
}
void xr_scene_submission_bind(uint64_t value,SceneSubmissionPtr snapshot) {
    if(!xr_scene_capture_enabled())return;
    std::lock_guard lock(capture_mutex);submissions.bind(value,std::move(snapshot));
}
SceneSubmissionPtr xr_scene_submission_current(){return active_submission;}
SceneSubmissionScope::SceneSubmissionScope(uint64_t value):previous_(std::move(active_submission)) {
    if(xr_scene_capture_enabled()){std::lock_guard lock(capture_mutex);active_submission=submissions.get(value);}
}
SceneSubmissionScope::~SceneSubmissionScope(){active_submission=std::move(previous_);}bool xr_scene_capture_enabled() {
    static const bool enabled=[] {const char* e=std::getenv("HOOKSHOT_VR_SCENE_CAPTURE");return e&&e[0]=='1';}();
    return enabled;
}
void xr_scene_camera_observation(const SceneCameraEvidence& draw,const float* block,size_t count) {
    if(!xr_scene_capture_enabled())return;
    std::lock_guard lock(capture_mutex);
    CameraCapturePair pair{};const bool fresh=capture.latest(now_ns(),pair);
    const auto result=match_scene_camera(block,count,draw,fresh?&pair:nullptr);
    const auto pinned=match_submitted_camera(active_submission.get(),now_ns(),draw,block,count);
    static uint64_t submitted_observed=0,submitted_matches=0,missing_context=0,ambiguous=0,equivalent=0;
    ++submitted_observed;if(pinned.matched)++submitted_matches;if(!active_submission)++missing_context;
    if(pinned.ambiguous)++ambiguous;if(pinned.equivalent>1)++equivalent;
    if(submitted_observed==1||submitted_observed%2000==0)
        logf("vr-submit-camera: candidates %llu, matched %llu, missing context %llu, ambiguous identities %llu, equivalent observations %llu",
            (unsigned long long)submitted_observed,(unsigned long long)submitted_matches,(unsigned long long)missing_context,
            (unsigned long long)ambiguous,(unsigned long long)equivalent);    static uint64_t totals[5]{},observed=0,matched_frames=0,last_match_frame=UINT64_MAX;
    ++totals[static_cast<unsigned>(result)];++observed;
    if(result==SceneCameraMatch::Match&&draw.replay_frame!=last_match_frame){++matched_frames;last_match_frame=draw.replay_frame;}
    // Aggregate evidence only. A matching candidate is not unique camera ownership or permission to override it.
    if(observed==1||observed%2000==0)logf("vr-scene: draw candidates %llu, invalid %llu, no fresh camera %llu, view mismatch %llu, projection mismatch %llu, matched %llu, matched replay frames %llu",
        (unsigned long long)observed,(unsigned long long)totals[0],(unsigned long long)totals[1],(unsigned long long)totals[2],
        (unsigned long long)totals[3],(unsigned long long)totals[4],(unsigned long long)matched_frames);
}}