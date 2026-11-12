// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "stereo_plan.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>
namespace hookshot::vr {
// Diagnostic CPU copy only. This does not establish shader-wide semantics, calibrate
// the game world, allocate GPU resources, or authorize submission of the result.
namespace uniform_detail {
inline bool close(double a,double b) {
    return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=0.02+std::abs(a)*0.0001;
}
inline bool rigid(const float* v) {
    for(int i=0;i<12;++i)if(!std::isfinite(v[i]))return false;
    for(int r=0;r<3;++r)for(int s=0;s<3;++s) {
        double dot=0;for(int c=0;c<3;++c)dot+=double(v[4*r+c])*v[4*s+c];
        if(std::abs(dot-(r==s?1:0))>0.002)return false;
    }
    double det=v[0]*(double(v[5])*v[10]-double(v[6])*v[9])-
        v[1]*(double(v[4])*v[10]-double(v[6])*v[8])+v[2]*(double(v[4])*v[9]-double(v[5])*v[8]);
    return std::abs(det-1)<0.003;
}
inline bool projection(const float* p) {
    for(int i=0;i<16;++i)if(!std::isfinite(p[i]))return false;
    for(int i:{1,3,4,7,8,9,12,13,15})if(std::abs(p[i])>0.000001)return false;
    return p[0]>0&&std::abs(p[5])>0.000001&&std::abs(p[14]+1)<0.000001&&std::abs(p[11])>0;
}
inline bool composed(const float* v,const float* p,const float* vp) {
    for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
        double expected=0;for(int k=0;k<4;++k)expected+=double(p[4*r+k])*(k==3?(c==3?1:0):v[4*k+c]);
        if(!close(expected,vp[4*r+c]))return false;
    }
    return true;
}
inline void inverse(const float* v,float* inverse) {
    for(int r=0;r<3;++r) {
        for(int c=0;c<3;++c)inverse[4*r+c]=v[4*c+r];
        double t=0;for(int k=0;k<3;++k)t-=double(v[4*k+r])*v[4*k+3];
        inverse[4*r+3]=float(t);
    }
}
inline bool narrow(double value,float& out) {
    if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return false;
    out=float(value);return true;
}
}
// Caller supplies the COMPLETE descriptor payload, not merely the audited prefix.
// The 0xb0 inverse-view layout requires the reviewed build/profile; unknown fields,
// including 0x1c0 and bytes beyond 0x230, remain byte-exact. No in-place operation.
inline bool prepare_eye_uniform_copy(const void* source,size_t bytes,const EyeMatrices& eye,
                                     ClipDepth clip,std::vector<std::byte>& out) {
    constexpr size_t prefix=0x230,maximum=1024*1024;
    if(!source||bytes<prefix||bytes>maximum||clip!=ClipDepth::MinusOneToOne)return false;
    const uintptr_t begin=reinterpret_cast<uintptr_t>(source),end=begin+bytes;
    if(end<begin)return false;
    if(!out.empty()) {
        const uintptr_t dest=reinterpret_cast<uintptr_t>(out.data());
        if(dest>std::numeric_limits<uintptr_t>::max()-out.size()||
           (begin<dest+out.size()&&dest<end))return false;
    }
    float original[0xe0/4];std::memcpy(original,source,sizeof original);
    if(!uniform_detail::rigid(original)||!uniform_detail::projection(original+0x70/4)||
       !uniform_detail::composed(original,original+0x70/4,original+0x30/4))return false;
    float expected_inverse[12];uniform_detail::inverse(original,expected_inverse);
    for(int i=0;i<12;++i)if(!uniform_detail::close(expected_inverse[i],original[0xb0/4+i]))return false;
    float patched[0xe0/4]{};
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)
        if(!uniform_detail::narrow(eye.view.rows[r][c],patched[4*r+c]))return false;
    for(int i=0;i<16;++i) {
        if(!uniform_detail::narrow(eye.world_to_clip[i],patched[0x30/4+i])||
           !uniform_detail::narrow(eye.projection[i],patched[0x70/4+i]))return false;
    }
    if(!uniform_detail::rigid(patched)||!uniform_detail::projection(patched+0x70/4)||
       !uniform_detail::composed(patched,patched+0x70/4,patched+0x30/4))return false;
    uniform_detail::inverse(patched,patched+0xb0/4);
    for(int r=0;r<3;++r)if(!uniform_detail::close(eye.view.eye[r],patched[0xb0/4+4*r+3]))return false;
    for(float value:patched)if(!std::isfinite(value))return false;
    std::vector<std::byte> candidate(bytes);
    std::memcpy(candidate.data(),source,bytes);
    std::memcpy(candidate.data(),patched,sizeof patched);
    out.swap(candidate);return true;
}
}
