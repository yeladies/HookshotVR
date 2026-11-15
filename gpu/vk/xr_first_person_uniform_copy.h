// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_first_person_rig.h"
#include "xr_eye_uniform_copy.h"
namespace mkwvk {
// Diagnostic primary-prefix copies only. Unknown fields and temporal/current
// aliases outside the prefix stay byte-exact; this is NOT a full-scene patch.
inline bool prepare_first_person_primary_uniform_copies(const void* source,size_t bytes,
    const FirstPersonProjectionPacket& packet,std::array<std::vector<std::byte>,2>& out){
    if(!source||bytes<0x230||bytes>65536||bytes%4||packet.game_present<0||!packet.native_depth_preserved)return false;
    const auto begin=reinterpret_cast<uintptr_t>(source);
    if(begin>UINTPTR_MAX-bytes)return false;
    for(const auto& old:out)if(!old.empty()){
        const auto dest=reinterpret_cast<uintptr_t>(old.data());
        if(dest>UINTPTR_MAX-old.size()||(begin<dest+old.size()&&dest<begin+bytes))return false;
    }
    const auto& tracking=packet.tracking;
    if(!tracking.running||!tracking.focused||!tracking.should_render||(tracking.head.flags&3)!=3)return false;
    for(const auto& eye:tracking.eyes)if((eye.pose.flags&3)!=3)return false;
    float prefix[0xe0/4];std::memcpy(prefix,source,sizeof prefix);
    const auto evidence=first_person_projection_evidence(packet,prefix,prefix+0x70/4);
    if(!evidence.available||!evidence.center_matches||!evidence.depth_exact||!evidence.fov_matches)return false;
    std::array<std::vector<std::byte>,2> next;
    for(unsigned eye=0;eye<2;++eye){
        const auto& camera=packet.camera.eyes[eye];hookshot::vr::EyeMatrices matrices{};
        for(unsigned r=0;r<3;++r){
            matrices.view.eye[r]=camera.world_from_eye[4*r+3];
            for(unsigned c=0;c<4;++c)matrices.view.rows[r][c]=camera.view[4*r+c];
        }
        for(unsigned k=0;k<16;++k){matrices.projection[k]=camera.projection[k];matrices.world_to_clip[k]=camera.world_to_clip[k];}
        if(!hookshot::vr::prepare_eye_uniform_copy(source,bytes,matrices,hookshot::vr::ClipDepth::MinusOneToOne,next[eye]))return false;
    }
    if(next[0]==next[1])return false;
    out.swap(next);return true;
}
}
