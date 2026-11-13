// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
namespace mkwvk {
struct XrShiftedDrawComparison {
    bool verified=false;std::array<uint64_t,3> changed{};std::array<double,3> x{},y{};
};
// Six tightly packed 32-bit color or D32 snapshots: before, original after, two exact baselines, two shifted
// draws. A mask-centroid check is a bounded raster diagnostic, not scene coverage.
inline XrShiftedDrawComparison compare_xr_shifted_draw(const void* data,size_t available,uint32_t width,uint32_t height){
    XrShiftedDrawComparison out;
    if(!data||width<160||height<1||width>4096||height>4096)return out;
    const uint64_t pixels=uint64_t(width)*height,frame=pixels*4;
    if(available!=6*frame)return out;
    const auto* bytes=static_cast<const unsigned char*>(data);
    if(std::memcmp(bytes+frame,bytes+2*frame,size_t(frame))||std::memcmp(bytes+frame,bytes+3*frame,size_t(frame)))return out;
    for(unsigned i=0;i<3;++i){const auto* image=bytes+(i?i+3:1)*frame;
        for(uint64_t p=0;p<pixels;++p)if(std::memcmp(bytes+p*4,image+p*4,4)){++out.changed[i];out.x[i]+=double(p%width);out.y[i]+=double(p/width);}
        if(out.changed[i]<16)return out;out.x[i]/=out.changed[i];out.y[i]/=out.changed[i];
    }
    for(unsigned i=1;i<3;++i){
        const double ratio=double(out.changed[i])/out.changed[0];
        if(ratio<.75||ratio>1.25||std::abs(out.y[i]-out.y[0])>1.25||std::abs(out.x[i]-out.x[0]-(i==1?-4:4))>1.25)return out;
    }
    out.verified=std::memcmp(bytes+4*frame,bytes+5*frame,size_t(frame))!=0;return out;
}
}
