// SPDX-License-Identifier: GPL-3.0-or-later
#include "projection_depth.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try {
    for(auto order:{DepthOrder::Forward,DepthOrder::Reversed})for(bool infinite:{false,true})for(bool flip:{false,true}){
        ProjectionMatrix matrix{};check(eye_projection({-.7,.8,.9,-.6},.125,infinite?std::numeric_limits<double>::infinity():200,ClipDepth::MinusOneToOne,order,flip,matrix),"authored projection");
        float source[16];for(int i=0;i<16;++i)source[i]=float(matrix[i]);ProjectionDepthRange result;
        check(nvn_projection_depth(source,16,ClipDepth::MinusOneToOne,result),"decode reviewed NVN depth");
        check(result.order==order&&std::abs(result.near_z-.125)<1e-6,"near and order");
        check(result.far_infinite_or_unresolved==infinite,"far classification");
        if(infinite)check(std::isinf(result.far_z),"infinite/unresolved far preserved");
        else check(std::abs(result.far_z-200)<.02,"float far uncertainty tolerated");
        const double near_ndc=(-double(source[10])*result.near_z+source[11])/result.near_z;
        check(std::abs(near_ndc-(order==DepthOrder::Forward?-1:1))<1e-10,"recovered near maps to correct endpoint");
        const auto before=result;
        auto reject=[&](const float* data,size_t n,ClipDepth clip){check(!nvn_projection_depth(data,n,clip,result)&&result.near_z==before.near_z&&result.far_z==before.far_z,"failure preserves previous depth range");};
        reject(source,15,ClipDepth::MinusOneToOne);reject(nullptr,16,ClipDepth::MinusOneToOne);reject(source,16,ClipDepth::ZeroToOne);
        float invalid[16];std::copy_n(source,16,invalid);invalid[10]=0;reject(invalid,16,ClipDepth::MinusOneToOne);
        invalid[10]=source[10];invalid[11]=-source[11];reject(invalid,16,ClipDepth::MinusOneToOne);
        invalid[11]=source[11];invalid[0]=std::numeric_limits<float>::quiet_NaN();reject(invalid,16,ClipDepth::MinusOneToOne);
    }
    ProjectionMatrix distant{};check(eye_projection({-.7,.7,.7,-.7},.1,1e12,ClipDepth::MinusOneToOne,DepthOrder::Forward,false,distant),"distant finite source");
    float rounded[16];for(int i=0;i<16;++i)rounded[i]=float(distant[i]);ProjectionDepthRange result;
    check(nvn_projection_depth(rounded,16,ClipDepth::MinusOneToOne,result)&&result.far_infinite_or_unresolved,"float rounding must not invent an exact far distance");
    std::cout<<"Reviewed NVN depth extraction preserves forward/reversed endpoints and far-plane uncertainty\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
