// SPDX-License-Identifier: GPL-3.0-or-later
#include "depth_pair_tracking.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void yaw(float* q,double a){q[0]=q[2]=0;q[1]=float(std::sin(a/2));q[3]=float(std::cos(a/2));}
DepthPairSource source(){DepthPairSource s;s.state=1;s.generation=9;s.origin=7;s.capture_pose=1000000000;s.width=1152;s.height=648;s.format=123;s.projection={1.4f,1.8f,-1.01f,-.2f};s.optical_center={.01f,-.02f};return s;}
int main(){try{
    auto s=source();mkwvk::ImmersiveTag captured;captured.valid=true;captured.generation=7;captured.observed_ns=1000000000;
    DepthPairTrackingTarget target;target.origin=7;target.serial=19;target.observed_ns=1010000000;
    for(unsigned eye=0;eye<2;++eye){auto& e=target.eyes[eye];e.orientation_valid=e.position_valid=true;e.position[0]=eye?.032f:-.032f;e.fov={-.7f,.8f,.75f,-.65f};}
    std::array<DepthPairParameters,2> out;check(prepare_tracked_depth_pair_parameters(s,captured,target,1020000000,800,600,out),"tracked pair accepted");
    check(std::abs(out[0].source_from_eye[3]+.032f)<1e-6&&std::abs(out[1].source_from_eye[3]-.032f)<1e-6,"physical eye translations preserve IPD and handedness");check(out[0].eye_tangents[0]!=-out[0].eye_tangents[1]&&out[0].optical[0]==.01f,"asymmetric runtime lens and source optical center remain separate");
    // Independent oracle: apply the existing engine camera override to a
    // nonidentity game view/anchor, then compose source view * inverse eye view.
    mkwvk::ImmersivePose anchor,cap;anchor.generation=cap.generation=7;anchor.observed_ns=cap.observed_ns=1000000000;yaw(anchor.q,-.4);anchor.p[0]=1;anchor.p[1]=1.2f;anchor.p[2]=.5f;
    yaw(cap.q,.1);cap.p[0]=1.1f;cap.p[1]=1.3f;cap.p[2]=.6f;std::memcpy(captured.orientation,cap.q,16);std::memcpy(captured.position,cap.p,12);
    const double head_yaw=.15;
    for(unsigned eye=0;eye<2;++eye){auto& e=target.eyes[eye];yaw(e.orientation.data(),head_yaw+(eye?.01:-.01));const double side=eye?.032:-.032;e.position={float(1.15+side*std::cos(head_yaw)),1.3f,float(.62-side*std::sin(head_yaw))};}
    check(prepare_tracked_depth_pair_parameters(s,captured,target,1020000000,800,600,out),"rotated/translated late eyes accepted");
    float base[12]={float(std::cos(.3)),0,float(std::sin(.3)),2,0,1,0,-1,float(-std::sin(.3)),0,float(std::cos(.3)),-4};float source_view[12];std::memcpy(source_view,base,48);mkwvk::ImmersiveCamera source_camera;check(source_camera.apply(1,source_view,cap,1020000000,&anchor),"source engine oracle");
    for(unsigned eye=0;eye<2;++eye){mkwvk::ImmersivePose located;located.generation=7;located.observed_ns=1010000000;std::memcpy(located.q,target.eyes[eye].orientation.data(),16);std::memcpy(located.p,target.eyes[eye].position.data(),12);float eye_view[12];std::memcpy(eye_view,base,48);mkwvk::ImmersiveCamera eye_camera;check(eye_camera.apply(1,eye_view,located,1020000000,&anchor),"target engine oracle");
        for(unsigned r=0;r<3;++r){double translation=source_view[4*r+3];for(unsigned c=0;c<3;++c){double rotation=0;for(unsigned k=0;k<3;++k)rotation+=double(source_view[4*r+k])*eye_view[4*c+k];check(std::abs(rotation-out[eye].source_from_eye[4*r+c])<1e-5,"base camera/anchor cancel against existing engine override");translation-=rotation*eye_view[4*c+3];}check(std::abs(translation-out[eye].source_from_eye[4*r+3])<1e-5,"room-space late translation matches engine camera composition");}}
    const auto valid=out;auto reject=[&](){out=valid;check(!prepare_tracked_depth_pair_parameters(s,captured,target,1020000000,800,600,out)&&std::memcmp(out.data(),valid.data(),sizeof out)==0,"rejection preserves both output cameras");};
    captured.menu=true;reject();captured.menu=false;captured.temporal_stereo=true;reject();captured.temporal_stereo=false;s.origin=8;reject();s.origin=7;s.capture_pose=1;reject();s.capture_pose=1000000000;
    target.origin=8;reject();target.origin=7;target.serial=0;reject();target.serial=19;target.observed_ns=1030000000;reject();target.observed_ns=900000000;reject();target.observed_ns=1010000000;
    target.eyes[1].position_valid=false;reject();target.eyes[1].position_valid=true;target.eyes[1].orientation={0,0,0,0};reject();yaw(target.eyes[1].orientation.data(),.16);target.eyes[1].position[0]=10;reject();target.eyes[1].position[0]=1.18f;target.eyes[1].fov[0]=.1f;reject();target.eyes[1].fov[0]=-.7f;
    check(!prepare_tracked_depth_pair_parameters(s,captured,target,1300000000,800,600,out),"expired capture or target rejected");
    std::cout<<"Tracked depth pair matches existing engine camera composition, physical/canted eyes and asymmetric FOV; rejects stale or crossed poses atomically\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
