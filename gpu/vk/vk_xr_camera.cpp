// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_submission.h"
#include "xr_image_lineage.h"
#include "xr_camera_block_capture.h"
#include <iomanip>
#include <string>
#include <fstream>
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
SceneSubmissionScope::~SceneSubmissionScope(){active_submission=std::move(previous_);}
bool xr_scene_capture_enabled() {
    static const bool enabled=[] {const char* e=std::getenv("HOOKSHOT_VR_SCENE_CAPTURE");return e&&e[0]=='1';}();
    return enabled;
}
bool xr_scene_camera_observation(const SceneCameraEvidence& draw,const float* block,size_t count) {
    if(!xr_scene_capture_enabled())return false;
    std::lock_guard lock(capture_mutex);
    CameraCapturePair pair{};const bool fresh=capture.latest(now_ns(),pair);
    const auto result=match_scene_camera(block,count,draw,fresh?&pair:nullptr);
    const auto pinned=match_submitted_camera(active_submission.get(),now_ns(),draw,block,count);
    if(pinned.matched)if(const char* path=std::getenv("HOOKSHOT_VR_CAMERA_BLOCKS_FILE")) {
        static CameraBlockCapture blocks;CameraBlockSample sample;
        static bool armed=false;static uint64_t checked_frame=UINT64_MAX;
        if(!armed&&checked_frame!=draw.replay_frame){checked_frame=draw.replay_frame;std::ifstream marker(std::string(path)+".armed");armed=bool(marker);}
        if(armed&&blocks.consider(draw.replay_frame,pinned.revision,draw.width,draw.height,block,count,sample)) {
            std::ofstream file(path,std::ios::app);
            if(file){file<<std::setprecision(9)<<"{\"schema\":1,\"frame\":"<<sample.frame<<",\"camera_revision\":"<<sample.revision
                <<",\"width\":"<<sample.width<<",\"height\":"<<sample.height<<",\"floats\":[";
                bool first=true;for(float value:sample.values){if(!first)file<<',';first=false;file<<value;}file<<"]}\n";}
        }
    }
    static uint64_t submitted_observed=0,submitted_matches=0,missing_context=0,ambiguous=0,equivalent=0;
    ++submitted_observed;if(pinned.matched)++submitted_matches;if(!active_submission)++missing_context;
    if(pinned.ambiguous)++ambiguous;if(pinned.equivalent>1)++equivalent;
    if(submitted_observed==1||submitted_observed%2000==0)
        logf("vr-submit-camera: candidates %llu, matched %llu, missing context %llu, ambiguous identities %llu, equivalent observations %llu",
            (unsigned long long)submitted_observed,(unsigned long long)submitted_matches,(unsigned long long)missing_context,
            (unsigned long long)ambiguous,(unsigned long long)equivalent);
    static uint64_t totals[5]{},observed=0,matched_frames=0,last_match_frame=UINT64_MAX;
    ++totals[static_cast<unsigned>(result)];++observed;
    if(result==SceneCameraMatch::Match&&draw.replay_frame!=last_match_frame){++matched_frames;last_match_frame=draw.replay_frame;}
    // Aggregate evidence only. A matching candidate is not unique camera ownership or permission to override it.
    if(observed==1||observed%2000==0)logf("vr-scene: draw candidates %llu, invalid %llu, no fresh camera %llu, view mismatch %llu, projection mismatch %llu, matched %llu, matched replay frames %llu",
        (unsigned long long)observed,(unsigned long long)totals[0],(unsigned long long)totals[1],(unsigned long long)totals[2],
        (unsigned long long)totals[3],(unsigned long long)totals[4],(unsigned long long)matched_frames);
    return pinned.matched;
}
namespace {
std::mutex lineage_mutex;FrameImageGraph lineage;uint64_t lineage_frame=UINT64_MAX;
}
bool xr_image_lineage_enabled(){static const bool on=[] {auto e=std::getenv("HOOKSHOT_VR_IMAGE_LINEAGE");return e&&e[0]=='1';}();return on;}
void xr_image_lineage_work(uint64_t frame,ImageWork work){
    if(!xr_image_lineage_enabled())return;
    std::lock_guard lock(lineage_mutex);
    if(frame!=lineage_frame){lineage.reset();lineage_frame=frame;}
    lineage.observe(std::move(work));
}
void xr_image_lineage_resource(uint64_t frame,ImageResourceDescription description){
    if(!xr_image_lineage_enabled())return;
    std::lock_guard lock(lineage_mutex);
    if(frame!=lineage_frame){lineage.reset();lineage_frame=frame;}
    lineage.describe(description);
}
void xr_image_lineage_present(uint64_t frame,uint64_t image){
    if(!xr_image_lineage_enabled())return;
    std::lock_guard lock(lineage_mutex);
    if(frame!=lineage_frame)return;
    const auto& s=lineage.stats();const bool connected=lineage.dependent(image);
    static uint64_t frames=0,seeded=0,connected_frames=0;static bool saved=false;
    ++frames;if(s.seeds)++seeded;if(connected)++connected_frames;
    if(frames%120==0||seeded==1)logf("vr-lineage: frames %llu, seeded %llu, present connected %llu; last events %llu, seeds %llu, dependent graphics %llu, dependent compute %llu, shared %llu, opaque %llu, overflow %d; replay ready 0",
        (unsigned long long)frames,(unsigned long long)seeded,(unsigned long long)connected_frames,(unsigned long long)s.events,
        (unsigned long long)s.seeds,(unsigned long long)s.dependent_graphics,(unsigned long long)s.dependent_compute,
        (unsigned long long)s.shared,(unsigned long long)s.opaque,int(s.overflow));
    const char* path=std::getenv("HOOKSHOT_VR_LINEAGE_FILE");
    if(saved||!path||seeded<30||!connected||s.overflow||s.seeds<100)return;
    std::ofstream file(path,std::ios::trunc);if(!file)return;
    file<<"{\"schema\":1,\"frame\":"<<frame<<",\"present_image\":"<<image<<",\"replay_ready\":false,\"coverage_complete\":false,\"events\":[";
    bool first=true;
    for(const auto& record:lineage.records()){
        if(!first)file<<',';first=false;
        file<<"{\"kind\":"<<int(record.work.kind)<<",\"program\":"<<record.work.program<<",\"seed\":"<<(record.work.camera_seed?"true":"false")
            <<",\"dependent\":"<<(record.dependent?"true":"false")<<",\"opaque_buffers\":"<<(record.work.opaque_buffers?"true":"false")
            <<",\"width\":"<<record.work.width<<",\"height\":"<<record.work.height<<",\"reads\":[";
        bool item=true;for(const auto& input:record.inputs){if(!item)file<<',';item=false;file<<'['<<input.image<<','<<input.version<<']';}
        file<<"],\"writes\":[";item=true;for(const auto& output:record.outputs){if(!item)file<<',';item=false;file<<'['<<output.image<<','<<output.version<<']';}file<<"],\"buffer_reads\":[";item=true;
        for(const auto& input:record.buffer_inputs){if(!item)file<<',';item=false;file<<'['<<input.range.object<<','<<input.range.offset<<','<<input.range.size<<','<<input.version<<']';}
        file<<"],\"buffer_writes\":[";item=true;
        for(const auto& output:record.buffer_outputs){if(!item)file<<',';item=false;file<<'['<<output.range.object<<','<<output.range.offset<<','<<output.range.size<<','<<output.version<<']';}
        file<<"]}";
    }
    file<<"],\"resources\":[";first=true;
    for(const auto& [id,r]:lineage.descriptions()){
        if(!first)file<<',';first=false;
        file<<"{\"image\":"<<id<<",\"allocation_bytes\":"<<r.allocation_bytes<<",\"format\":"<<r.format<<",\"width\":"<<r.width
            <<",\"height\":"<<r.height<<",\"depth\":"<<r.depth<<",\"levels\":"<<r.levels<<",\"layers\":"<<r.layers<<",\"samples\":"<<r.samples
            <<",\"aspect\":"<<r.aspect<<",\"type\":"<<r.type<<",\"sparse\":"<<(r.sparse?"true":"false")<<'}';
    }
    file<<"]}\n";file.close();saved=bool(file);if(saved)logf("vr-lineage: private representative frame saved; incomplete coverage, no automatic stereo replay");
}
}