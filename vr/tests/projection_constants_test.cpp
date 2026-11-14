// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_projection_constants.h"
#include <array>
#include <stdexcept>
#include <iostream>
#include <limits>
using namespace mkwvk;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
    std::array<float,76> b{};b[56]=.1f;b[57]=25000;b[38]=-(b[57]+b[56])/(b[57]-b[56]);b[39]=-2*b[57]*b[56]/(b[57]-b[56]);b[64]=b[57]-b[56];b[72]=1152;b[73]=648;b[74]=1.f/1152;b[75]=1.f/648;
    const auto original=b;check(xr_projection_constants_valid(b.data(),b.size(),1152,648),"near/far/projection and viewport identities");
    for(auto index:{38,39,56,57,64,72,73,74,75}){b=original;b[index]=std::numeric_limits<float>::quiet_NaN();check(!xr_projection_constants_valid(b.data(),b.size(),1152,648),"nonfinite field rejected");}
    for(auto index:{38,39,64,74,75}){b=original;b[index]+=index==64?100.f:1.f;check(!xr_projection_constants_valid(b.data(),b.size(),1152,648),"inconsistent scalar cannot authorize fragment patch");}
    b=original;b[57]=.05;check(!xr_projection_constants_valid(b.data(),b.size(),1152,648),"invalid clip interval");
    b=original;check(!xr_projection_constants_valid(b.data(),75,1152,648),"short source");
    check(!xr_projection_constants_valid(b.data(),76,1280,720),"actual render target dimensions required");
    check(!xr_projection_constants_valid(nullptr,76,1152,648),"null source");
    std::cout<<"Projection and viewport scalar identities checked without modifying source data\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
