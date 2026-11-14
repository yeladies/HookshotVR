// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_shifted_draw_check.h"
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace mkwvk;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    constexpr size_t frame=160*32*4;std::vector<unsigned char> pixels(frame*6);
    auto fill=[&](unsigned slot,int dx,int dy=0){for(int y=8+dy;y<12+dy;++y)for(int x=70+dx;x<78+dx;++x)pixels[slot*frame+(y*160+x)*4]=7;};
    auto valid=[&](){pixels.assign(frame*6,0);fill(1,0);fill(2,0);fill(3,0);fill(4,-4);fill(5,4);};
    auto compare=[&](){return compare_xr_shifted_draw(pixels.data(),pixels.size(),160,32);};
    valid();auto result=compare();check(result.verified&&result.changed[0]==32&&result.x[1]-result.x[0]==-4&&result.x[2]-result.x[0]==4,"opposite four-pixel shifts");
    valid();pixels[2*frame]^=1;check(!compare().verified,"baseline mismatch cannot become a transformed success");
    valid();std::memcpy(pixels.data()+4*frame,pixels.data()+5*frame,frame);check(!compare().verified,"same direction for both eyes rejected");
    valid();std::memset(pixels.data()+4*frame,0,frame);fill(4,-4,3);check(!compare().verified,"vertical displacement rejected");
    valid();std::memcpy(pixels.data()+4*frame,pixels.data()+frame,frame);check(!compare().verified,"unchanged private view rejected");
    valid();std::memset(pixels.data()+5*frame,1,frame);check(!compare().verified,"widespread corruption rejected");
    pixels.assign(frame*6,0);check(!compare().verified,"invisible reference is inconclusive");
    valid();check(!compare_xr_shifted_draw(pixels.data(),pixels.size()-1,160,32).verified,"truncated readback rejected");
    check(!compare_xr_shifted_draw(nullptr,pixels.size(),160,32).verified&&!compare_xr_shifted_draw(pixels.data(),pixels.size(),4097,32).verified,"invalid bounds rejected");
    valid();check(compare_xr_shifted_draw(pixels.data(),pixels.size(),160,32,true).verified,"translated displacement control");
    valid();std::memset(pixels.data()+4*frame,0,frame);std::memset(pixels.data()+5*frame,0,frame);fill(4,-2);fill(5,2);
    check(compare_xr_shifted_draw(pixels.data(),pixels.size(),160,32,true).verified&&!compare().verified,"translated mode is not a fixed lens-shift oracle");
    valid();std::memset(pixels.data()+5*frame,0,frame);fill(5,10);
    check(!compare_xr_shifted_draw(pixels.data(),pixels.size(),160,32,true).verified,"unbalanced translated corruption rejected");
    std::cout<<"Shifted raster masks require exact baselines, visible output and opposite bounded displacement\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
