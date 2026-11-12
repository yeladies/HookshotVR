// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_uniform_copy.h"
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
EyeMatrices camera(float x,float yaw=0) {
    EyeMatrices eye{};TrackingOrigin origin{};origin.yaw_radians=yaw;
    const double angle=0.2;
    XrPosef pose{{float(std::sin(angle)),0,0,float(std::cos(angle))},{x,2,3}};
    check(tracked_camera_view(origin,pose,XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,eye.view),"tracked camera");
    check(eye_projection({-.7,.8,.9,-.6},.05,1000,ClipDepth::MinusOneToOne,DepthOrder::Forward,false,eye.projection),"projection");
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)for(int k=0;k<4;++k)
        eye.world_to_clip[4*r+c]+=eye.projection[4*r+k]*(k==3?(c==3?1:0):eye.view.rows[k][c]);
    return eye;
}
int main(){try {
    auto base=camera(0),left=camera(-.032f,.3f),right=camera(.032f,.3f);
    std::vector<std::byte> source(0x780);
    for(size_t i=0;i<source.size();++i)source[i]=std::byte((i*37)%256);
    float block[0xe0/4]{};
    for(int r=0;r<3;++r)for(int c=0;c<4;++c)block[4*r+c]=float(base.view.rows[r][c]);
    for(int i=0;i<16;++i){block[0x30/4+i]=float(base.world_to_clip[i]);block[0x70/4+i]=float(base.projection[i]);}
    uniform_detail::inverse(block,block+0xb0/4);std::memcpy(source.data(),block,sizeof block);
    const auto before=source;std::vector<std::byte> l,r;
    check(prepare_eye_uniform_copy(source.data(),source.size(),left,ClipDepth::MinusOneToOne,l),"left complete copy");
    check(prepare_eye_uniform_copy(source.data(),source.size(),right,ClipDepth::MinusOneToOne,r),"right complete copy");
    check(source==before&&l.size()==source.size()&&r.size()==source.size(),"original and full extent preserved");
    check(l!=r,"distinct eye matrices");
    for(size_t i=0xe0;i<source.size();++i)check(l[i]==source[i]&&r[i]==source[i],"unknown payload bytes preserved");
    float actual[0xe0/4];std::memcpy(actual,l.data(),sizeof actual);
    // Independent affine inverse product, including pitched/yawed translation.
    for(int row=0;row<3;++row)for(int col=0;col<4;++col) {
        double value=col==3?actual[4*row+3]:0;
        for(int k=0;k<3;++k)value+=double(actual[4*row+k])*actual[0xb0/4+4*k+col];
        check(std::abs(value-(row==col?1:0))<1e-5,"view times inverse is identity");
    }
    const auto good=l;
    auto reject=[&](const void* data,size_t bytes,const EyeMatrices& eye,ClipDepth clip) {
        check(!prepare_eye_uniform_copy(data,bytes,eye,clip,l)&&l==good,"failure must preserve previous complete copy");
    };
    reject(nullptr,source.size(),left,ClipDepth::MinusOneToOne);
    reject(source.data(),0x22f,left,ClipDepth::MinusOneToOne);
    reject(source.data(),1024*1024+1,left,ClipDepth::MinusOneToOne);
    reject(source.data(),source.size(),left,ClipDepth::ZeroToOne);
    reject(l.data(),l.size(),left,ClipDepth::MinusOneToOne);
    auto bad=left;bad.view.rows[0][0]=2;reject(source.data(),source.size(),bad,ClipDepth::MinusOneToOne);
    bad=left;bad.world_to_clip[3]+=1;reject(source.data(),source.size(),bad,ClipDepth::MinusOneToOne);
    bad=left;bad.view.eye[0]+=1;reject(source.data(),source.size(),bad,ClipDepth::MinusOneToOne);
    bad=left;bad.projection[0]=std::numeric_limits<double>::infinity();reject(source.data(),source.size(),bad,ClipDepth::MinusOneToOne);
    for(size_t offset:{size_t(0),size_t(0x30),size_t(0x70),size_t(0xb0)}) {
        auto corrupt=source;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(corrupt.data()+offset,&nan,4);
        reject(corrupt.data(),corrupt.size(),left,ClipDepth::MinusOneToOne);
    }
    auto corrupt=source;float wrong=12;std::memcpy(corrupt.data()+0xb0+12,&wrong,4);
    reject(corrupt.data(),corrupt.size(),left,ClipDepth::MinusOneToOne);
    std::cout<<"Private full-payload eye copies preserve unknown fields and reject incoherent matrices\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
