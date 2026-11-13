// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_eye_uniform_copy.h"
namespace mkwvk {
// Source before/after and two unbound private patches. Does not validate a game draw.
inline bool xr_matrix_patch_payloads(const unsigned char* data,size_t bytes,size_t range){
    using namespace hookshot::vr;
    if(!data||range<0x230||range>65536||range%4||bytes!=4*range||std::memcmp(data,data+range,range))return false;
    float base[56];std::memcpy(base,data,sizeof base);
    float translations[2]{};
    for(unsigned eye=0;eye<2;++eye){
        EyeMatrices matrix{};for(int r=0;r<3;++r)for(int c=0;c<4;++c)matrix.view.rows[r][c]=base[4*r+c];
        translations[eye]=float(base[3]+(eye?.01f:-.01f));matrix.view.rows[0][3]=translations[eye];
        float view[12],inverse[12];for(int i=0;i<12;++i)view[i]=float(matrix.view.rows[i/4][i%4]);
        uniform_detail::inverse(view,inverse);for(int r=0;r<3;++r)matrix.view.eye[r]=inverse[4*r+3];
        for(int i=0;i<16;++i)matrix.projection[i]=base[28+i];
        for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)
            matrix.world_to_clip[4*r+c]+=matrix.projection[4*r+k]*(k==3?(c==3?1:0):matrix.view.rows[k][c]);
        std::vector<std::byte> expected;if(!prepare_eye_uniform_copy(data,range,matrix,ClipDepth::MinusOneToOne,expected))return false;
        const auto* actual=data+(eye+2)*range;
        if(std::memcmp(actual,expected.data(),0x30)||std::memcmp(actual+0x70,data+0x70,0x40)||std::memcmp(actual+0xe0,data+0xe0,range-0xe0))return false;
        float got[56],wanted[56];std::memcpy(got,actual,sizeof got);std::memcpy(wanted,expected.data(),sizeof wanted);
        for(int i=12;i<56;++i)if(!std::isfinite(got[i])||std::abs(got[i]-wanted[i])>0.00003f*(1+std::abs(wanted[i])))return false;
    }
    return translations[0]!=translations[1]; // A rounded-away displacement is inconclusive.
}
}
