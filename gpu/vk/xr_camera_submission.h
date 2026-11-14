// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_scene_camera.h"
#include <array>
#include <memory>
#include <map>
namespace mkwvk {
struct TimedSceneCamera {CameraCapturePair pair;int64_t received_ns=0;};
struct SceneSubmissionSnapshot {
    int64_t captured_ns=0;
    std::array<TimedSceneCamera,16> cameras{};size_t count=0;
};
// Bounded observations, not proof of the game's simulation tick. Caller synchronizes.
class SceneCameraHistory {
    SceneSubmissionSnapshot history_{};
public:
    void clear(){history_={};}
    void observe(const CameraCapturePair& pair,int64_t at) {
        if(!pair.revision||at<=0)return;
        for(size_t i=0;i<history_.count;++i)
            if(history_.cameras[i].pair.primary.sequence==pair.primary.sequence&&
               history_.cameras[i].pair.uniforms.camera_id==pair.uniforms.camera_id) {
                history_.cameras[i]={pair,at};return;
            }
        if(history_.count==history_.cameras.size()) {
            for(size_t i=1;i<history_.count;++i)history_.cameras[i-1]=history_.cameras[i];
            --history_.count;
        }
        history_.cameras[history_.count++]={pair,at};
    }
    SceneSubmissionSnapshot snapshot(int64_t at) const {
        SceneSubmissionSnapshot out{};out.captured_ns=at;
        for(size_t i=0;i<history_.count;++i) {
            const auto& camera=history_.cameras[i];
            if(camera.received_ns>0&&at>=camera.received_ns&&at-camera.received_ns<=250000000)
                out.cameras[out.count++]=camera;
        }
        return out;
    }
};
struct SubmittedCameraMatch {bool matched=false,ambiguous=false;uint32_t equivalent=0;uint64_t revision=0,camera_id=0;};
inline SubmittedCameraMatch match_submitted_uniform(const SceneSubmissionSnapshot* snapshot,int64_t now,
                                                    uint32_t width,uint32_t height,const float* block,size_t count) {
    SubmittedCameraMatch out{};
    if(!snapshot||snapshot->captured_ns<=0||now<snapshot->captured_ns||now-snapshot->captured_ns>500000000)return out;
    // All candidates share this copied block: validate shape/coherence once,
    // then retain individual freshness, identity and exact camera comparisons.
    if(match_camera_uniform(block,count,width,height,nullptr)!=SceneCameraMatch::NoCamera)return out;
    for(size_t i=0;i<snapshot->count&&i<snapshot->cameras.size();++i) {
        const auto& c=snapshot->cameras[i];
        if(c.received_ns<=0||c.received_ns>snapshot->captured_ns||snapshot->captured_ns-c.received_ns>250000000)continue;
        if(match_validated_camera_pair(block,&c.pair)!=SceneCameraMatch::Match)continue;
        if(out.camera_id&&out.camera_id!=c.pair.primary.camera_id){out.ambiguous=true;out.matched=false;return out;}
        out.camera_id=c.pair.primary.camera_id;out.revision=c.pair.revision;++out.equivalent;out.matched=true;
    }
    return out;
}
inline SubmittedCameraMatch match_submitted_camera(const SceneSubmissionSnapshot* snapshot,int64_t now,
                                                   const SceneCameraEvidence& draw,const float* block,size_t count) {
    if(!draw.color||!draw.depth||!draw.uniform_address||!draw.program)return {};
    return match_submitted_uniform(snapshot,now,draw.width,draw.height,block,count);
}
using SceneSubmissionPtr=std::shared_ptr<const SceneSubmissionSnapshot>;
class SceneSubmissionStore {
    std::map<uint64_t,SceneSubmissionPtr> entries_;
public:
    void bind(uint64_t value,SceneSubmissionPtr snapshot) {
        if(!value)return;
        entries_[value]=std::move(snapshot);
        while(entries_.size()>256)entries_.erase(entries_.begin());
    }
    SceneSubmissionPtr get(uint64_t value) const {auto i=entries_.find(value);return i==entries_.end()?nullptr:i->second;}
};
SceneSubmissionPtr xr_scene_submission_capture();
SceneSubmissionPtr xr_scene_submission_current();
void xr_scene_submission_bind(uint64_t,SceneSubmissionPtr);
// Thread-local scope names the source NVN submit, never a merged Vulkan recording's timeline tag.
class SceneSubmissionScope {
    SceneSubmissionPtr previous_;
public:
    explicit SceneSubmissionScope(uint64_t value);
    ~SceneSubmissionScope();
    SceneSubmissionScope(const SceneSubmissionScope&)=delete;
    SceneSubmissionScope& operator=(const SceneSubmissionScope&)=delete;
};
}
