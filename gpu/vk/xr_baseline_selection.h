// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <charconv>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace mkwvk {
struct XrBaselineStage {uint32_t nvn_stage=0,vk_stage=0;uint64_t fingerprint=0,words=0;};
struct XrBaselineProgram {uint64_t program=0;std::vector<XrBaselineStage> stages;};
struct XrBaselineSelection {std::string nonce,source;std::vector<XrBaselineProgram> programs;};
inline bool xr_lower_hex(std::string_view value,size_t count){
    if(value.size()!=count)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;
}
inline bool xr_selection_number(std::istringstream& input,uint64_t& out,bool hex=false){
    std::string value;if(!(input>>value)||value.empty()||value.size()>(hex?16:10))return false;
    if(hex){if(!xr_lower_hex(value,16))return false;}
    else for(char c:value)if(c<'0'||c>'9')return false;
    uint64_t candidate=0;const auto result=std::from_chars(value.data(),value.data()+value.size(),candidate,hex?16:10);
    if(result.ec!=std::errc{}||result.ptr!=value.data()+value.size())return false;out=candidate;return true;
}
// Selection scopes a baseline-only experiment. It is not permission for camera/scene
// replay; live state and module identities must still be checked at the actual draw.
inline bool parse_xr_baseline_selection(std::string_view text,std::string_view nonce,std::string_view source,XrBaselineSelection& out){
    if(text.size()>8192||!xr_lower_hex(nonce,32)||!xr_lower_hex(source,8))return false;
    std::istringstream input{std::string(text)};std::string magic;XrBaselineSelection next;
    if(!(input>>magic>>next.nonce>>next.source)||magic!="HSVR_BASELINE_DRAW_V1"||next.nonce!=nonce||next.source!=source)return false;
    uint64_t count=0;if(!xr_selection_number(input,count)||!count||count>32)return false;
    for(uint64_t i=0;i<count;++i){XrBaselineProgram program;uint64_t stages=0;
        if(!xr_selection_number(input,program.program,true)||!program.program||!xr_selection_number(input,stages)||stages!=2)return false;
        for(const auto& previous:next.programs)if(previous.program==program.program)return false;
        uint32_t mask=0;
        for(uint64_t stage=0;stage<stages;++stage){uint64_t ns=0,vk=0;XrBaselineStage item;
            if(!xr_selection_number(input,ns)||ns>=8||!xr_selection_number(input,vk)||(vk!=1&&vk!=16)||(mask&vk)||
               !xr_selection_number(input,item.fingerprint,true)||!xr_selection_number(input,item.words)||item.words<5||item.words>2*1024*1024)return false;
            item.nvn_stage=uint32_t(ns);item.vk_stage=uint32_t(vk);mask|=uint32_t(vk);program.stages.push_back(item);
        }
        if(mask!=17)return false;next.programs.push_back(std::move(program));
    }
    std::string extra;if(input>>extra)return false;out=std::move(next);return true;
}
}
