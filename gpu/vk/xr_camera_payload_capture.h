// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_uniform_copy_source.h"
#include <cstring>
#include <cstddef>
#include <utility>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
namespace mkwvk {
struct XrCameraPayloadEvidence {
    uint64_t serial=0,program=0,recording=0,guest=0;XrUniformSourceStamp stamp{};
    uint32_t width=0,height=0;bool completed=false,aliases=false,matrices=false,indirect=false;
};
// Preserve every bit, including non-float payload words. Metadata describes an
// already completed GPU source copy; it cannot independently establish provenance.
inline bool xr_camera_payload_json(const void* before,const void* after,size_t bytes,const XrCameraPayloadEvidence& e,std::string& out){
    if(!before||!after||bytes<560||bytes>65536||bytes%4||!e.serial||!e.program||!e.recording||!e.guest||e.guest%4||
       e.stamp.recording!=e.recording||!e.stamp.pool_generation||!e.stamp.capture_generation||!e.width||!e.height||e.width>4096||e.height>4096||
       !e.completed||!e.aliases||!e.matrices||!e.indirect||std::memcmp(before,after,bytes))return false;
    if(e.guest>UINT64_MAX-bytes||!((e.guest>=0x1000000000ull&&e.guest+bytes<=0x1200000000ull)||(e.guest>=0x7f00000000ull&&e.guest+bytes<=0x8000000000ull)))return false;
    std::ostringstream text;text<<"{\"schema\":1,\"kind\":\"completed_gpu_camera_payload\",\"serial\":"<<e.serial<<",\"program\":"<<e.program
        <<",\"recording\":"<<e.recording<<",\"guest\":"<<e.guest<<",\"pool_generation\":"<<e.stamp.pool_generation<<",\"capture_generation\":"<<e.stamp.capture_generation
        <<",\"width\":"<<e.width<<",\"height\":"<<e.height<<",\"bytes\":"<<bytes
        <<",\"gpu_completion_verified\":true,\"source_unchanged\":true,\"camera_aliases_verified\":true,\"matrix_payloads_verified\":true,\"indirect_guard_verified\":true,\"words\":[";
    const auto* source=static_cast<const unsigned char*>(before);
    for(size_t i=0;i<bytes;i+=4){uint32_t word=0;std::memcpy(&word,source+i,4);if(i)text<<',';text<<word;}
    text<<"]}\n";auto result=text.str();if(result.size()>256*1024)return false;out=std::move(result);return true;
}
inline bool write_xr_camera_payload(const char* path,const void* before,const void* after,size_t bytes,const XrCameraPayloadEvidence& e){
    if(!path||!*path)return false;
    try{std::string text;if(!xr_camera_payload_json(before,after,bytes,e,text))return false;
        std::ofstream file(std::filesystem::u8path(path),std::ios::binary|std::ios::trunc);if(!file)return false;
        file.write(text.data(),std::streamsize(text.size()));file.close();return bool(file);
    }catch(...){return false;}
}
}
