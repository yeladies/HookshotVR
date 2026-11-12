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
struct State {std::mutex mutex;std::map<uint64_t,Module> modules;std::set<uint64_t> digests,programs;uint64_t bytes=0;};
// Process lifetime, matching renderer workers; no CRT teardown of a mutex they may still reach.
State& state(){static auto* value=new State;return *value;}
const char* directory(){static const char* path=std::getenv("HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR");return path;}
std::string name(uint64_t digest){std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<digest;return s.str();}
uint64_t digest_words(const uint32_t* words,size_t count){uint64_t hash=14695981039346656037ull;
    for(size_t i=0;i<count;++i)for(unsigned byte=0;byte<4;++byte){hash^=(words[i]>>(byte*8))&255;hash*=1099511628211ull;}return hash;}
}
bool xr_shader_uniform_audit_enabled(){return directory()&&*directory();}
bool xr_shader_uniform_audit_module(uint64_t module,const uint32_t* words,size_t count){
    if(!xr_shader_uniform_audit_enabled()||!module||!words||count<5||count>2*1024*1024||words[0]!=0x07230203)return false;
    auto& s=state();std::lock_guard lock(s.mutex);
    if(!s.modules.count(module)&&s.modules.size()>=4096)return false;
    const uint64_t digest=digest_words(words,count);bool written=s.digests.count(digest)!=0;
    if(!written&&s.bytes+count*4<=64ull*1024*1024){
        std::ofstream file(std::filesystem::u8path(directory())/(name(digest)+".spv"),std::ios::binary|std::ios::trunc);
        if(file){file.write(reinterpret_cast<const char*>(words),std::streamsize(count*4));file.close();written=bool(file);
            if(written){s.digests.insert(digest);s.bytes+=count*4;}}
    }
    s.modules[module]={digest,count,written};return written;
}
bool xr_shader_uniform_audit_wants_program(uint64_t program){
    if(!xr_shader_uniform_audit_enabled()||!program)return false;
    auto& s=state();std::lock_guard lock(s.mutex);return s.programs.size()<64&&!s.programs.count(program);
}
bool xr_shader_uniform_audit_program(uint64_t program,const std::vector<ShaderUniformAuditBinding>& bindings){
    if(!xr_shader_uniform_audit_enabled()||!program)return false;
    auto& s=state();std::lock_guard lock(s.mutex);
    if(s.programs.count(program)||s.programs.size()>=64)return false;
    s.programs.insert(program);
    if(bindings.empty()||bindings.size()>128)return false;
    std::ofstream file(std::filesystem::u8path(directory())/"program-bindings.jsonl",std::ios::app);if(!file)return false;
    file<<"{\"schema\":1,\"program\":"<<program<<",\"bindings\":[";bool first=true;
    for(const auto& binding:bindings){
        if(!first)file<<',';first=false;
        const auto found=s.modules.find(binding.module);const bool available=found!=s.modules.end()&&found->second.written;
        file<<"{\"module\":";
        if(available)file<<'\"'<<name(found->second.digest)<<'\"';else file<<"null";
        file<<",\"nvn_stage\":"<<binding.nvn_stage<<",\"vk_stage\":"<<binding.vk_stage<<",\"binding\":"<<binding.binding
            <<",\"array_element\":"<<binding.array_element<<",\"bank\":"<<binding.bank<<",\"range_bytes\":"<<binding.range_bytes
            <<",\"primary_camera_binding\":"<<(binding.primary_camera_binding?"true":"false")<<'}';
    }
    file<<"]}\n";file.close();return bool(file);
}
}
