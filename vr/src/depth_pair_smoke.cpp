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
    const double a=2*(x+.5)/p.dimensions[2]-1,b=1-2*(y+.5)/p.dimensions[3];const auto& r=p.source_from_eye;
    const double dx=r[0]*a+r[1]*b-r[2],dy=r[4]*a+r[5]*b-r[6],dz=r[8]*a+r[9]*b-r[10];
    const double t=(-2.-r[11])/dz;const double sx=r[3]+dx*t,sy=r[7]+dy*t;
    const int px=int(std::floor((double(p.projection[0])*sx/2+1)*(p.dimensions[0]/2.)));const int py=int(std::floor((1-double(p.projection[1])*sy/2)*(p.dimensions[1]/2.)));
    require(px>=0&&px<int(p.dimensions[0])&&py>=0&&py<int(p.dimensions[1]),"analytic plane remains within overscan");return {unsigned(px),unsigned(py)};
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
    // A wider rectangular source feeding square eyes must not acquire a grid
    // of holes merely because nearest source texels span more target pixels.
    {DepthPairGpuFixture aspect(80,40,64,64,direct);std::array<DepthPairParameters,2> cameras{parameters(0,0),parameters(1,0)};
        for(auto& p:cameras){p.dimensions={80,40,64,64};p.projection[0]=p.projection[1]=.9f;p.optical[3]=.85f;}
        const float depth=float((1.-double(cameras[0].projection[2])+double(cameras[0].projection[3])/2)/2);std::vector<uint32_t> source(6400);
        for(unsigned y=0;y<40;++y)for(unsigned x=0;x<80;++x){const auto i=y*80+x;source[i]=x*4|(y*4<<9)|(128u<<18)|(15u<<27);source[3200+i]=float_word(depth);}
        const auto result=aspect.run(source,cameras,5);require(result.source==source,"aspect case preserves full source");
        for(unsigned eye=0;eye<2;++eye){require(result.counters[eye][0]==4096&&result.counters[eye][1]==0&&result.counters[eye][2]==0&&result.counters[eye][3]==0,"rectangular-source/square-eye plane must have complete coverage without quantization stripes");
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){const auto p=analytic_source_pixel(cameras[eye],x,y);require(result.pixels[y*128+eye*64+x]==expected_color(p[0],p[1],false),"aspect-scaled pixel disagrees with independent plane oracle");}}
    }
    // Alternating near/far columns deliberately make some iterative depth
    // lookups oscillate. Resolution independence must not disable rejection.
    {std::array<DepthPairParameters,2> cameras{parameters(0,0),parameters(1,0)};for(auto& p:cameras)p.optical[3]=.85f;std::vector<uint32_t> source(12800);
        for(unsigned y=0;y<80;++y)for(unsigned x=0;x<80;++x){const auto i=y*80+x;const double z=x%2?4:1;source[i]=x*4|(y*4<<9)|(128u<<18)|(15u<<27);source[6400+i]=float_word(float((1.-double(cameras[0].projection[2])+double(cameras[0].projection[3])/z)/2));}
        const auto result=fixture.run(source,cameras,6);require(result.source==source,"discontinuous-depth source unchanged");uint64_t residuals=0;
        for(const auto& c:result.counters){require(uint64_t(c[0])+c[1]+c[2]+c[3]==4096&&c[2]==0,"every discontinuous-depth pixel accounted without invented invalid depth");residuals+=c[3];}require(residuals>0,"nonconverging depth discontinuities still rejected");for(auto word:result.pixels)require((word>>24)==255,"discontinuity output completely written");
    }
    // Filtered reconstruction must reproduce an analytic continuous linear
    // ramp, not the old nearest-neighbour staircase. Both packed HDR formats.
    for(unsigned round=0;round<2;++round){std::array<DepthPairParameters,2> cameras{parameters(0,round),parameters(1,round)};
        for(auto& p:cameras){p.settings[2]=1;p.projection[0]=.83f;p.projection[1]=.79f;}const float depth=float((1.-double(cameras[0].projection[2])+double(cameras[0].projection[3])/2)/2);std::vector<uint32_t> source(12800);
        for(unsigned y=0;y<80;++y)for(unsigned x=0;x<80;++x){const unsigned i=y*80+x;source[i]=round?mini_float(float(x)/128,6)|(mini_float(float(y)/128,6)<<11)|(mini_float(.25f,5)<<22):x*4|(y*4<<9)|(128u<<18)|(15u<<27);source[6400+i]=float_word(depth);}
        const auto result=fixture.run(source,cameras,7+round);require(result.source==source,"filtered reconstruction preserves source packet");unsigned different=0;
        for(unsigned eye=0;eye<2;++eye){require(result.counters[eye][0]==4096,"filtered planar coverage is complete");const auto& p=cameras[eye];
            for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){const double ray_x=2*(x+.5)/64-1,ray_y=1-2*(y+.5)/64;const double sx=p.source_from_eye[3]+2*ray_x,sy=2*ray_y;
                const double px=(p.projection[0]*sx/2+1)*40-.5,py=(1-p.projection[1]*sy/2)*40-.5;const auto actual=result.pixels[y*128+eye*64+x];
                const int red=int(std::floor(px*255/128+.5)),green=int(std::floor(py*255/128+.5));require(std::abs(int(actual&255)-red)<=1&&std::abs(int((actual>>8)&255)-green)<=1&&((actual>>16)&255)==64,"filtered GPU color agrees with continuous independent plane oracle");
                const auto nearest=analytic_source_pixel(p,x,y);if(actual!=expected_color(nearest[0],nearest[1],false))++different;
            }}require(different>1000,"filter actually removes nearest-neighbour staircase");
    }
    // A red foreground in front of a blue background must stay red or blue:
    // four-tap color filtering cannot blur across the depth discontinuity.
    {std::array<DepthPairParameters,2> cameras{parameters(0,0),parameters(1,0)};for(auto& p:cameras)p.settings[2]=1;std::vector<uint32_t> source(12800);
        for(unsigned y=0;y<80;++y)for(unsigned x=0;x<80;++x){const auto i=y*80+x;const bool foreground_pixel=x<40;const double z=foreground_pixel?1:4;source[i]=(foreground_pixel?384u:0u)|(foreground_pixel?0u:(384u<<18))|(15u<<27);source[6400+i]=float_word(float((1.-double(cameras[0].projection[2])+double(cameras[0].projection[3])/z)/2));}
        const auto result=fixture.run(source,cameras,9);require(result.source==source,"depth-edge filtering preserves source packet");unsigned foreground=0,background=0;
        for(auto word:result.pixels){const auto r=word&255,b=(word>>16)&255;require(!(r&&b),"depth-aware filter cannot mix red foreground with blue background");if(r)++foreground;if(b)++background;}
        require(foreground>2000&&background>2000,"both sides of depth edge retain real color");
    }
    std::cout<<"same_source_depth_pair=true; direct_buffer_copy="<<(direct?"true":"false")<<"; GPU="<<fixture.gpu.props.deviceName<<"; target_pixels_checked=73728; depth_aware_filter_verified=true; unequal_aspect_verified=true; discontinuity_rejection_verified=true; source_formats_checked=2; rotated_plane_verified=true; invalid_depth_rejected=true; original_color_depth_unchanged=true; intermediate_CPU_pixels=false; submissions=9; allocations_reused=true; XR=false; game=false\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
