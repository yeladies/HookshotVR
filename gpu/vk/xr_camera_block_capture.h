// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
namespace mkwvk {
struct CameraBlockSample {uint64_t frame=0,revision=0;uint32_t width=0,height=0;std::array<float,0x230/4> values{};};
// Private diagnostic samples only. Production caller supplies an accepted submit-time camera match.
class CameraBlockCapture {
    std::array<CameraBlockSample,8> samples_{};size_t count_=0;
public:
    bool consider(uint64_t frame,uint64_t revision,uint32_t width,uint32_t height,const float* block,size_t count,CameraBlockSample& out) {
        if(!revision||!width||!height||!block||count<0x230/4||count_==samples_.size())return false;
        for(size_t i=0;i<0x230/4;++i)if(!std::isfinite(block[i]))return false;
        for(size_t i=0;i<count_;++i) {
            bool same=true;
            for(int k=0;k<12;++k)if(std::abs(double(block[k])-samples_[i].values[k])>(k%4==3?0.001:0.00001))same=false;
            if(same)return false;
        }
        CameraBlockSample sample{};sample.frame=frame;sample.revision=revision;sample.width=width;sample.height=height;
        for(size_t i=0;i<sample.values.size();++i)sample.values[i]=block[i];
        samples_[count_++]=sample;out=sample;return true;
    }
};
}
