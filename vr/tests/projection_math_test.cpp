// SPDX-License-Identifier: GPL-3.0-or-later
#include "projection_math.h"
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-8;}
double clip(const ProjectionMatrix& p,int row,double x,double y,double z){return p[row*4]*x+p[row*4+1]*y+p[row*4+2]*z+p[row*4+3];}
int main(){try {
    const FovAngles f{-0.65,0.85,0.75,-0.55};
    for(auto depth:{ClipDepth::MinusOneToOne,ClipDepth::ZeroToOne})for(auto order:{DepthOrder::Forward,DepthOrder::Reversed})for(bool flip:{false,true}) {
        ProjectionMatrix p{};check(eye_projection(f,0.05,1000,depth,order,flip,p),"finite projection");
        check(near(clip(p,0,std::tan(f.left),0,-1),-1)&&near(clip(p,0,std::tan(f.right),0,-1),1),"asymmetric horizontal frustum edges");
        check(near(clip(p,1,0,std::tan(f.up),-1),flip?-1:1)&&near(clip(p,1,0,std::tan(f.down),-1),flip?1:-1),"vertical edges and explicit flip");
        const double first=depth==ClipDepth::MinusOneToOne?-1:0;
        const double n=order==DepthOrder::Forward?first:1,far=order==DepthOrder::Forward?1:first;
        check(near(clip(p,2,0,0,-0.05)/0.05,n)&&near(clip(p,2,0,0,-1000)/1000,far),"near/far depth mapping");
        check(eye_projection(f,0.05,std::numeric_limits<double>::infinity(),depth,order,flip,p),"infinite far");
        check(near(clip(p,2,0,0,-0.05)/0.05,n)&&std::abs(clip(p,2,0,0,-1e12)/1e12-far)<1e-10,"infinite far limit");
    }
    ProjectionMatrix gl{},vk{};
    check(eye_projection(f,0.1,200,ClipDepth::MinusOneToOne,DepthOrder::Forward,false,gl)&&
          eye_projection(f,0.1,200,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk),"both clip conventions");
    for(double z:{-0.1,-0.7,-20.0,-200.0})check(near((clip(gl,2,0,0,z)/-z+1)/2,clip(vk,2,0,0,z)/-z),"NVN depth conversion applied exactly once");
    const double left_x=0.032,right_x=-0.032;
    check(clip(vk,0,left_x,0,-2)>clip(vk,0,right_x,0,-2),"eye separation produces opposing parallax");
    const auto saved=vk;
    check(!eye_projection(f,0,200,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk)&&vk==saved,"invalid near leaves output untouched");
    check(!eye_projection(f,1,0.5,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk),"invalid far");
    check(!eye_projection({0.2,0.1,0.8,-0.8},0.1,100,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk),"reversed angles");
    check(!eye_projection({-1.5707963267948966,0.5,0.5,-0.5},0.1,100,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk),"singular FOV rejected");
    check(!eye_projection({-0.5,0.5,std::numeric_limits<double>::quiet_NaN(),-0.5},0.1,100,ClipDepth::ZeroToOne,DepthOrder::Forward,false,vk),"NaN FOV rejected");
    std::cout<<"Per-eye projection conventions checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}