// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_internal.h"
#include "vk_shader.h"
#include "xr_baseline_draw.h"
#include "xr_baseline_selection.h"
#include "xr_shader_uniform_audit.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
namespace mkwvk {
namespace {
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_BASELINE_DRAW_FILE");return path&&*path?path:nullptr;}
struct State {
    std::mutex mutex;XrBaselineSelection selection;XrBaselineRecorder* current=nullptr;
    std::set<uint64_t> attempted;uint64_t last_frame=UINT64_MAX,last_poll_frame=UINT64_MAX;bool stopped=false;
    unsigned completed=0;uint64_t current_program=0;XrBaselineComparison result;
};
State& state(){static auto* value=new State;return *value;}
void report(const State& p,const char* status){
    std::ofstream attempts(std::filesystem::u8path(std::string(output())+".attempts.jsonl"),std::ios::app);
    if(attempts)attempts<<"{\"program\":\""<<std::hex<<p.current_program<<std::dec<<"\",\"status\":\""<<status<<"\",\"changed_pixels\":"<<p.result.changed_pixels<<",\"left_mismatched_pixels\":"<<p.result.mismatched_pixels[0]<<",\"right_mismatched_pixels\":"<<p.result.mismatched_pixels[1]<<"}\n";
    std::ofstream file(std::filesystem::u8path(output()),std::ios::trunc);
    if(file)file<<"{\"schema\":1,\"status\":\""<<status<<"\",\"attempts\":"<<p.attempted.size()<<",\"completed_attempts\":"<<p.completed
        <<",\"changed_original_pixels\":"<<p.result.changed_pixels<<",\"left_mismatched_pixels\":"<<p.result.mismatched_pixels[0]
        <<",\"right_mismatched_pixels\":"<<p.result.mismatched_pixels[1]<<",\"resources_released\":"<<(!p.current?"true":"false")
        <<",\"matrix_probe_requested\":"<<(p.result.matrix_probe_requested?"true":"false")<<",\"matrix_probe_validated\":"<<(p.result.matrix_probe_validated?"true":"false")
        <<",\"matrix_source_unchanged\":"<<(p.result.matrix_source_unchanged?"true":"false")<<",\"matrix_payload_bytes\":"<<p.result.matrix_payload_bytes<<",\"patched_buffers_bound_to_draw\":false"
        <<",\"unchanged_camera_baseline\":true,\"full_scene_stereo\":false,\"headset_presentation\":false}\n";
}
void selection(State& p){
    const char* path=std::getenv("HOOKSHOT_VR_BASELINE_SELECTION_FILE"),*nonce=std::getenv("HOOKSHOT_VR_BASELINE_NONCE"),*source=std::getenv("HOOKSHOT_VR_BASELINE_SOURCE");
    if(!path||!nonce||!source)return;
    std::ifstream file(std::filesystem::u8path(path),std::ios::binary);if(!file)return;
    std::string text(8193,'\0');file.read(text.data(),text.size());const auto size=file.gcount();if(size<=0||size>8192)return;text.resize(size);
    XrBaselineSelection next;if(parse_xr_baseline_selection(text,nonce,source,next))p.selection=std::move(next);
}
bool selected(const State& p,const Program& program){
    for(const auto& candidate:p.selection.programs)if(candidate.program==program.key){
        if(program.stages.size()!=candidate.stages.size())return false;
        for(const auto& stage:program.stages){bool matched=false;uint64_t fingerprint=0;size_t words=0;
            if(!xr_shader_uniform_audit_identity(uint64_t(stage.module),fingerprint,words))return false;
            for(const auto& expected:candidate.stages)if(expected.nvn_stage==uint32_t(stage.nvn_stage)&&expected.vk_stage==uint32_t(stage.vk_stage)&&expected.fingerprint==fingerprint&&expected.words==words)matched=true;
            if(!matched)return false;
        }
        return true;
    }
    return false;
}
}
bool xr_baseline_draw_enabled(){static const bool on=env_on("RX_VK_XR_BASELINE_DRAW")&&output();return on;}
void xr_baseline_draw_tick(uint64_t frame,uint64_t serial){
    if(!xr_baseline_draw_enabled())return;auto& p=state();std::lock_guard lock(p.mutex);
    if(p.current){
        xr_baseline_recorder_abandon(p.current); // Finished attempts remain finished; skipped sources become abandoned.
        XrBaselineComparison result;
        if(p.last_poll_frame!=frame){p.last_poll_frame=frame;
        if(xr_baseline_recorder_retire(p.current,result)){
            p.result=result;++p.completed;
            if(result.result==XrBaselineComparison::Result::Match){p.stopped=true;report(p,result.matrix_probe_requested&&!result.matrix_probe_validated?"matrix_patch_unverified":"baseline_matched");}
            else if(result.result==XrBaselineComparison::Result::Mismatch){p.stopped=true;report(p,"baseline_mismatch");}
            else report(p,"attempt_inconclusive");
        }}
    }
    if(p.stopped||p.current||p.last_frame==frame)return;p.last_frame=frame;
    if(p.attempted.size()>=8){p.stopped=true;report(p,"attempt_budget_exhausted");return;}
    selection(p);(void)serial;
}
bool xr_baseline_draw_selected(const Program& program){
    if(!xr_baseline_draw_enabled())return false;auto& p=state();std::lock_guard lock(p.mutex);
    return !p.stopped&&!p.current&&p.attempted.size()<8&&!p.attempted.count(program.key)&&selected(p,program);
}
void xr_baseline_draw_prepare(const XrBaselineRecorderInput& input){
    if(!xr_baseline_draw_enabled())return;auto& p=state();std::lock_guard lock(p.mutex);
    if(p.stopped||p.current||p.attempted.size()>=8||p.attempted.count(input.program))return;
    auto requested=input;requested.matrix_probe=env_on("RX_VK_XR_MATRIX_PATCH_PROBE");
    p.current=xr_baseline_recorder_create(requested);if(!p.current)return;
    p.attempted.insert(input.program);p.current_program=input.program;
    if(!xr_baseline_recorder_prepare(p.current))xr_baseline_recorder_abandon(p.current);
    report(p,"attempt_prepared");
}
void xr_baseline_draw_finish(uint64_t serial,uint64_t recording,uint64_t program,VkCommandBuffer command,const std::vector<VkWriteDescriptorSet>& writes){
    if(!xr_baseline_draw_enabled())return;auto& p=state();std::lock_guard lock(p.mutex);if(!p.current)return;
    if(xr_baseline_recorder_finish(p.current,serial,recording,program,command,writes))report(p,"private_draws_recorded");
}
}
