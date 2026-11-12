// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_block_capture.h"
#include <stdexcept>
#include <limits>
#include <iostream>
using namespace mkwvk;
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
int main(){try{CameraBlockCapture capture;CameraBlockSample out;float block[0x230/4]{};
    block[0]=block[5]=block[10]=1;const auto first=block[0];
    check(capture.consider(1,1,1280,720,block,0x230/4,out),"initial copied sample");
    check(out.values[0]==first&&out.frame==1,"immutable authored payload");
    check(!capture.consider(2,2,1280,720,block,0x230/4,out),"same view is not another distinct pose");
    for(int i=1;i<8;++i){block[3]=float(i);check(capture.consider(i+1,i+1,1280,720,block,0x230/4,out),"different view captured");}
    block[3]=9;check(!capture.consider(9,9,1280,720,block,0x230/4,out),"capture bound is eight samples");
    CameraBlockCapture invalid;block[1]=std::numeric_limits<float>::quiet_NaN();
    check(!invalid.consider(1,1,1280,720,block,0x230/4,out),"nonfinite packet excluded");
    block[1]=0;check(!invalid.consider(1,0,1280,720,block,0x230/4,out),"unmatched camera cannot be sampled");
    check(!invalid.consider(1,1,1280,720,block,2,out),"short block excluded");
    std::cout<<"Private bounded distinct-view uniform capture checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
