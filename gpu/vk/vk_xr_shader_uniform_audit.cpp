// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_shader_uniform_audit.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
namespace mkwvk {
namespace {
struct Module {uint64_t digest=0;size_t words=0;bool written=false;};
struct State {std::mutex mutex;std::map<uint64_t,Module> modules;std::set<uint64_t> digests,programs;uint64_t bytes=0,metadata_bytes=0;};
// Process lifetime, matching renderer workers; no CRT teardown of a mutex they may still reach.
State& state(){static auto* value=new State;return *value;}
const char* directory(){static const char* path=std::getenv("HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR");return path;}
std::string name(uint64_t digest){std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<digest;return s.str();}
uint64_t digest_words(const uint32_t* words,size_t count){uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<count;++i)for(unsigned byte=0;byte<4;++byte){hash^=(words[i]>>(byte*8))&255;hash*=1099511628211ull;}return hash;}
}
bool xr_shader_uniform_audit_enabled(){return directory()&&*directory();}
bool xr_shader_uniform_audit_compute_only(){static const bool value=[]{const char* p=std::getenv("HOOKSHOT_VR_SHADER_UNIFORM_COMPUTE_ONLY");return p&&p[0]=='1';}();return value;}
bool xr_shader_uniform_audit_module(uint64_t module,const uint32_t* words,size_t count){
    if(!xr_shader_uniform_audit_enabled()||!module||!words||count<5||count>2*1024*1024||words[0]!=0x07230203)return false;
    auto& s=state();std::lock_guard lock(s.mutex);
    if(!s.modules.count(module)&&s.modules.size()>=4096)return false;
    const uint64_t digest=digest_words(words,count);bool written=s.digests.count(digest)!=0;
    if(!written&&s.bytes+count*4<=64ull*1024*1024){
        s.bytes+=count*4; // Reserve even on an I/O failure; partial files count toward the cap.
        std::ofstream file(std::filesystem::u8path(directory())/(name(digest)+".spv"),std::ios::binary|std::ios::trunc);
        if(file){file.write(reinterpret_cast<const char*>(words),std::streamsize(count*4));file.close();written=bool(file);
            if(written){s.digests.insert(digest);}}
    }
    s.modules[module]={digest,count,written};return written;
}
bool xr_shader_uniform_audit_identity(uint64_t module,uint64_t& fingerprint,size_t& words){
    if(!xr_shader_uniform_audit_enabled()||!module)return false;
    auto& s=state();std::lock_guard lock(s.mutex);const auto found=s.modules.find(module);
    if(found==s.modules.end()||!found->second.written)return false;
    fingerprint=found->second.digest;words=found->second.words;return true;
}
bool xr_shader_uniform_audit_wants_program(uint64_t program){
    if(!xr_shader_uniform_audit_enabled()||!program)return false;
    auto& s=state();std::lock_guard lock(s.mutex);return s.programs.size()<64&&!s.programs.count(program);
}
bool xr_shader_uniform_audit_program(uint64_t program,const std::vector<ShaderUniformAuditBinding>& bindings,const std::vector<ShaderUniformAuditStage>& stages){
    if(!xr_shader_uniform_audit_enabled()||!program)return false;
    auto& s=state();std::lock_guard lock(s.mutex);
    if(s.programs.count(program)||s.programs.size()>=64)return false;
    s.programs.insert(program);
    if((bindings.empty()&&stages.empty())||bindings.size()>128||stages.size()>8)return false;
    uint32_t mask=0;
    for(const auto& stage:stages){
        if(!stage.module||stage.nvn_stage>=8||!stage.vk_stage||(stage.vk_stage&(stage.vk_stage-1))||stage.vk_stage>128||(mask&stage.vk_stage))return false;
        mask|=stage.vk_stage;
    }
    if(!stages.empty())for(const auto& b:bindings){bool found=false;
        for(const auto& stage:stages)if(stage.module==b.module&&stage.vk_stage==b.vk_stage&&stage.nvn_stage==b.nvn_stage)found=true;
        if(!found)return false;
    }
    for(const auto& b:bindings){
        auto valid=[&](const std::vector<uint32_t>& values,uint32_t size){
            if(values.size()>64)return false;
            for(auto offset:values)if(offset%16||offset>b.range_bytes||size>b.range_bytes-offset)return false;
            return values.empty()||b.camera_candidates.eligible;
        };
        if(!valid(b.camera_candidates.view_offsets,48)||!valid(b.camera_candidates.projection_offsets,64)||
           !valid(b.camera_candidates.vp_offsets,64))return false;
    }
    std::ostringstream file;
    file<<"{\"schema\":1,\"program\":"<<program<<",\"stage_inventory_complete\":"<<(!stages.empty()?"true":"false")<<",\"stages\":[";bool first=true;
    for(const auto& stage:stages){
        if(!first)file<<',';first=false;
        const auto found=s.modules.find(stage.module);const bool available=found!=s.modules.end()&&found->second.written;
        file<<"{\"module\":";if(available)file<<'\"'<<name(found->second.digest)<<'\"';else file<<"null";
        file<<",\"nvn_stage\":"<<stage.nvn_stage<<",\"vk_stage\":"<<stage.vk_stage
            <<",\"uses_global_memory\":"<<(stage.uses_global_memory?"true":"false")
            <<",\"writes_storage_buffers\":"<<(stage.writes_storage_buffers?"true":"false")
            <<",\"writes_storage_images\":"<<(stage.writes_storage_images?"true":"false")
            <<",\"writes_texel_buffers\":"<<(stage.writes_texel_buffers?"true":"false")<<'}';
    }
    file<<"],\"bindings\":[";first=true;
    for(const auto& binding:bindings){
        if(!first)file<<',';first=false;
        const auto found=s.modules.find(binding.module);const bool available=found!=s.modules.end()&&found->second.written;
        file<<"{\"module\":";
        if(available)file<<'\"'<<name(found->second.digest)<<'\"';else file<<"null";
        file<<",\"nvn_stage\":"<<binding.nvn_stage<<",\"vk_stage\":"<<binding.vk_stage<<",\"binding\":"<<binding.binding
            <<",\"array_element\":"<<binding.array_element<<",\"bank\":"<<binding.bank<<",\"range_bytes\":"<<binding.range_bytes
            <<",\"primary_camera_binding\":"<<(binding.primary_camera_binding?"true":"false")
            <<",\"candidate_scan_eligible\":"<<(binding.camera_candidates.eligible?"true":"false")
            <<",\"candidate_scan_truncated\":"<<(binding.camera_candidates.truncated?"true":"false")
            <<",\"ambiguous_candidate_offsets\":"<<binding.camera_candidates.ambiguous_offsets;
        auto offsets=[&](const char* field,const std::vector<uint32_t>& values){file<<",\""<<field<<"\":[";bool first=true;for(auto value:values){if(!first)file<<',';first=false;file<<value;}file<<']';};
        offsets("candidate_view_offsets",binding.camera_candidates.view_offsets);
        offsets("candidate_projection_offsets",binding.camera_candidates.projection_offsets);
        offsets("candidate_vp_offsets",binding.camera_candidates.vp_offsets);file<<'}';
    }
    file<<"]}\n";const auto payload=file.str();
    if(payload.size()>4*1024*1024-s.metadata_bytes)return false;
    s.metadata_bytes+=payload.size(); // Reserve failed/partial writes too.
    std::ofstream output(std::filesystem::u8path(directory())/"program-bindings.jsonl",std::ios::app);
    if(!output)return false;output<<payload;output.close();
    return bool(output);
}
}
