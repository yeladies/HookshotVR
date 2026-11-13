// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_shader_uniform_audit.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace mkwvk;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    check(argc==2,"mode required");const std::string mode=argv[1];
    const uint32_t words[]={0x07230203,0x00010500,0,16,0,0x00010000};
    if(mode=="off"){
        check(!xr_shader_uniform_audit_enabled()&&!xr_shader_uniform_audit_module(1,words,6)&&!xr_shader_uniform_audit_wants_program(7),"disabled audit is inactive");return 0;
    }
    check(mode=="enabled"&&xr_shader_uniform_audit_enabled(),"audit explicitly enabled");
    auto path=std::filesystem::u8path(std::getenv("HOOKSHOT_VR_SHADER_UNIFORM_AUDIT_DIR"));
    check(path.filename()=="shader-audit-test"&&!std::filesystem::is_symlink(path),"test output is a named build child, never a link");
    check(std::filesystem::weakly_canonical(path.parent_path())==std::filesystem::canonical(std::filesystem::current_path()),"test output stays inside build directory");
    std::filesystem::create_directories(path);
    check(xr_shader_uniform_audit_module(1,words,6)&&xr_shader_uniform_audit_module(2,words,6),"bounded authored modules exported");
    check(!xr_shader_uniform_audit_module(3,nullptr,6)&&!xr_shader_uniform_audit_module(3,words,0),"invalid payload rejected");
    std::vector<ShaderUniformAuditBinding> bindings{{1,0,1,6,0,3,560,true},{2,1,16,9,0,4,64,false}};
    const std::vector<ShaderUniformAuditStage> stages{{1,0,1,false,false,false,false},{2,1,16,false,false,false,false}};
    bindings[0].camera_candidates.eligible=true;bindings[0].camera_candidates.view_offsets={256};
    auto invalid=bindings;invalid[0].camera_candidates.view_offsets.assign(65,256);
    check(!xr_shader_uniform_audit_program(80,invalid),"candidate output bound enforced before serialization");
    check(xr_shader_uniform_audit_wants_program(7),"first emitted camera program wanted");
    check(xr_shader_uniform_audit_program(7,bindings,stages)&&!xr_shader_uniform_audit_wants_program(7),"program metadata emitted once");
    check(!xr_shader_uniform_audit_program(7,bindings),"no duplicate program record");
    std::ifstream file(path/"program-bindings.jsonl");std::string line,last;while(std::getline(file,line))if(!line.empty())last=line;
    check(last.find("\"primary_camera_binding\":true")!=std::string::npos&&last.find("\"binding\":6")!=std::string::npos,"binding provenance and index retained");
    check(last.find("\"candidate_view_offsets\":[256]")!=std::string::npos,"candidate offsets retained without matrix contents");
    check(last.find("\"stage_inventory_complete\":true")!=std::string::npos&&last.find("\"stages\":[")!=std::string::npos,"explicit stage inventory retained");
    check(xr_shader_uniform_audit_program(8,{bindings[0]},stages),"fragment stage without uniform bindings exported");
    check(xr_shader_uniform_audit_program(9,{},stages),"stage-only program remains representable");
    auto bad_stages=stages;bad_stages.push_back(stages[0]);check(!xr_shader_uniform_audit_program(20,bindings,bad_stages),"duplicate Vulkan stage rejected");
    check(!xr_shader_uniform_audit_program(21,bindings,{stages[0]}),"binding cannot refer to an omitted stage");
    auto missing=stages;missing[1].module=999;check(xr_shader_uniform_audit_program(10,{bindings[0]},missing),"missing module remains explicit instead of fabricated");
    std::ifstream updated(path/"program-bindings.jsonl");std::string latest;while(std::getline(updated,line))if(!line.empty())latest=line;
    check(latest.find("\"module\":null")!=std::string::npos,"unavailable stage module is null");
    const auto start=last.find("\"module\":\"");check(start!=std::string::npos,"module fingerprint available");
    const auto fingerprint=last.substr(start+10,16);check(std::filesystem::file_size(path/(fingerprint+".spv"))==sizeof words,"module file has unchanged authored byte length");
    for(uint64_t i=100;i<170;++i)xr_shader_uniform_audit_program(i,bindings);
    check(!xr_shader_uniform_audit_wants_program(9999),"program export bounded");
    std::cout<<"Optional local shader export, binding provenance and bounds checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
