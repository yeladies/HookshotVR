// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
namespace mkwvk {
// Only for view translations with an unchanged projection and viewport. These
// scalar identities are verified against the submitted bytes, not an offset guess.
inline bool xr_projection_constants_valid(const float* b,size_t count,uint32_t width,uint32_t height){
    if(!b||count<76||!width||!height||width>16384||height>16384)return false;
    for(int i:{38,39,56,57,64,72,73,74,75})if(!std::isfinite(b[i]))return false;
    const double n=b[56],f=b[57];if(n<=0||f<=n||f>1e8||b[72]!=float(width)||b[73]!=float(height))return false;
    const auto close=[](double a,double v){return std::abs(a-v)<=.00001+std::abs(v)*.0001;};
    return close(b[38],-(f+n)/(f-n))&&close(b[39],-2*f*n/(f-n))&&close(b[64],f-n)&&
        std::abs(double(b[74])-1./width)<=1e-9+1e-5/width&&std::abs(double(b[75])-1./height)<=1e-9+1e-5/height;
}
}
