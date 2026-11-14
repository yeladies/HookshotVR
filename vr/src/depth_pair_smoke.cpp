// SPDX-License-Identifier: GPL-3.0-or-later
// Authored inputs only. No game, OpenXR session, window or display mutation.
#define VK_NO_PROTOTYPES
#include "depth_pair_gpu_fixture.h"
#include <iostream>
using namespace hookshot::vr;
using namespace hookshot::vr::smoke;
namespace {
uint32_t float_word(float f){uint32_t v;std::memcpy(&v,&f,4);return v;}
uint32_t mini_float(float f,unsigned mantissa){if(!f)return 0;const auto bits=float_word(f);return (((bits>>23)&255)-112)<<mantissa|((bits&0x7fffff)>>(23-mantissa));}
uint32_t expected_color(unsigned x,unsigned y,bool flip){if(flip)x=79-x;auto byte=[](unsigned v){return uint32_t(std::floor(double(v)*255/128+.5));};return byte(x)|(byte(y)<<8)|(64u<<16)|0xff000000u;}
DepthPairParameters parameters(unsigned eye,unsigned round){DepthPairParameters p;const float angle=round==2?.07f:0,co=std::cos(angle),si=std::sin(angle);
    p.source_from_eye={co,0,si,eye?.0625f:-.0625f,0,1,0,0,-si,0,co,round==2?.05f:0};
    p.eye_tangents={-1,1,1,-1};p.projection={.8f,.8f,float(-10.1/9.9),float(-2./9.9)};p.optical={0,0,100,1.25f};p.dimensions={80,80,64,64};p.settings={round==1?122u:123u,4,0,0};return p;
}
std::array<unsigned,2> analytic_source_pixel(const DepthPairParameters& p,unsigned x,unsigned y){
    const double a=2*(x+.5)/64-1,b=1-2*(y+.5)/64;const auto& r=p.source_from_eye;
    const double dx=r[0]*a+r[1]*b-r[2],dy=r[4]*a+r[5]*b-r[6],dz=r[8]*a+r[9]*b-r[10];
    const double t=(-2.-r[11])/dz;const double sx=r[3]+dx*t,sy=r[7]+dy*t;
    const int px=int(std::floor((double(p.projection[0])*sx/2+1)*40));const int py=int(std::floor((1-double(p.projection[1])*sy/2)*40));
    require(px>=0&&px<80&&py>=0&&py<80,"analytic plane remains within overscan");return {unsigned(px),unsigned(py)};
}
}
int main(int argc,char** argv){if(argc!=2||(std::strcmp(argv[1],"--run")&&std::strcmp(argv[1],"--run-direct"))){std::cerr<<"Explicit native GPU opt-in: --run or --run-direct\n";return 2;}
try{const bool direct=std::strcmp(argv[1],"--run-direct")==0;DepthPairGpuFixture fixture(80,80,64,64,direct);
    for(unsigned round=0;round<4;++round){
        std::vector<uint32_t> source(12800);const bool flip=round==1;
        const float depth=float((1.-double(parameters(0,round).projection[2])+double(parameters(0,round).projection[3])/2)/2);
        for(unsigned y=0;y<80;++y)for(unsigned x=0;x<80;++x){const unsigned sx=flip?79-x:x,index=y*80+x;
            source[index]=round==1?mini_float(float(sx)/128,6)|(mini_float(float(y)/128,6)<<11)|(mini_float(.25f,5)<<22):sx*4|(y*4<<9)|(128u<<18)|(15u<<27);
            source[6400+index]=round==3?0x7fc00001u:float_word(depth);
        }
        const std::array<DepthPairParameters,2> cameras{parameters(0,round),parameters(1,round)};
        const auto result=fixture.run(source,cameras,round+1);require(result.source==source,"original GPU packet unchanged");
        for(unsigned eye=0;eye<2;++eye){const auto& counters=result.counters[eye];
            require(counters[0]==(round==3?0u:4096u)&&counters[1]==0&&counters[2]==(round==3?4096u:0u)&&counters[3]==0,"complete plane coverage or complete invalid-depth rejection");
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){const uint32_t actual=result.pixels[y*128+eye*64+x];uint32_t expected=0xff000000u;
                if(round!=3){const auto coordinate=analytic_source_pixel(cameras[eye],x,y);expected=expected_color(coordinate[0],coordinate[1],flip);}
                for(unsigned channel=0;channel<4;++channel){const int error=int((actual>>(channel*8))&255)-int((expected>>(channel*8))&255);require(std::abs(error)<=(round==2&&channel<2?2:0),"GPU stereo pixel disagrees with independent ray/plane oracle");}
            }
        }
    }
    std::cout<<"same_source_depth_pair=true; direct_buffer_copy="<<(direct?"true":"false")<<"; GPU="<<fixture.gpu.props.deviceName<<"; target_pixels_checked=32768; source_formats_checked=2; rotated_plane_verified=true; invalid_depth_rejected=true; original_color_depth_unchanged=true; intermediate_CPU_pixels=false; submissions=4; allocations_reused=true; XR=false; game=false\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
