// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_scene_camera.h"
#include <array>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace mkwvk;
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
int main(){try {
    std::array<float,0x230/4> block{};
    block[0]=block[5]=block[10]=1;block[3]=-2;
    float* p=block.data()+0x70/4;p[0]=1;p[5]=1.7f;p[10]=-1.01f;p[11]=-0.2f;p[14]=-1;
    block[0x120/4]=1280;block[0x124/4]=720;
    auto multiply=[&]{for(int r=0;r<4;++r)for(int c=0;c<4;++c){double v=0;for(int k=0;k<4;++k)v+=p[4*r+k]*(k==3?(c==3?1:0):block[4*k+c]);block[0x30/4+4*r+c]=float(v);}};
    multiply();CameraCapturePair pair{};pair.revision=1;
    std::copy_n(block.data(),12,pair.primary.view);std::copy_n(p,16,pair.uniforms.projection);
    const SceneCameraEvidence draw{8,123,9,10,0x1000,1280,720};
    const auto original=block;
    auto match=[&](const CameraCapturePair* candidate){return match_scene_camera(block.data(),block.size(),draw,candidate);};
    check(match(&pair)==SceneCameraMatch::Match,"submitted perspective camera matches");
    check(block==original,"diagnostic never modifies uniform bytes");
    check(match(nullptr)==SceneCameraMatch::NoCamera,"missing/expired observation cannot establish ownership");
    pair.primary.view[3]=-3;check(match(&pair)==SceneCameraMatch::ViewMismatch,"different camera excluded");pair.primary.view[3]=-2;
    pair.uniforms.projection[0]=2;check(match(&pair)==SceneCameraMatch::ProjectionMismatch,"different projection excluded");pair.uniforms.projection[0]=1;
    p[2]=0.001f;p[6]=-0.001f;multiply();check(match(&pair)==SceneCameraMatch::Match,"existing subpixel jitter accepted");
    p[2]=0.02f;multiply();check(match(&pair)==SceneCameraMatch::ProjectionMismatch,"large frustum change excluded");block=original;
    block[0x30/4+3]+=1;check(match(&pair)==SceneCameraMatch::Invalid,"incoherent VP rejected");block=original;
    block[0x120/4]=512;check(match(&pair)==SceneCameraMatch::Invalid,"other target dimensions excluded");block=original;
    block[0]=std::numeric_limits<float>::quiet_NaN();check(match(&pair)==SceneCameraMatch::Invalid,"nonfinite matrix excluded");block=original;
    auto missing=draw;missing.depth=0;check(match_scene_camera(block.data(),block.size(),missing,&pair)==SceneCameraMatch::Invalid,"missing depth excluded");
    check(match_scene_camera(block.data(),10,draw,&pair)==SceneCameraMatch::Invalid,"short block rejected");
    std::cout<<"Read-only submitted scene-camera evidence checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
