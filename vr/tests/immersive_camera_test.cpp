// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_immersive_camera.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace mkwvk;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
bool near(float a,float b){return std::abs(a-b)<.0001f;}
int main(){try{
    const float base[12]={1,0,0,-10,0,1,0,-2,0,0,1,-3};float view[12];std::memcpy(view,base,sizeof view);
    ImmersiveCamera camera;ImmersivePose pose;pose.generation=1;pose.observed_ns=1000000000;pose.p[1]=1.6f;
    check(camera.apply(7,view,pose,pose.observed_ns),"initial valid anchor");check(std::memcmp(view,base,sizeof view)==0,"anchor preserves game camera");
    pose.p[0]=.2f;pose.observed_ns+=10000000;check(camera.apply(7,view,pose,pose.observed_ns),"translation");check(near(view[3],-10.2f),"head translation changes native camera");
    check(camera.apply(7,view,pose,pose.observed_ns)&&near(view[3],-10.2f),"revisited camera does not accumulate overrides");
    const double angle=.3;pose.q[1]=float(std::sin(angle/2));pose.q[3]=float(std::cos(angle/2));pose.observed_ns+=10000000;
    check(camera.apply(7,view,pose,pose.observed_ns),"yaw");check(near(view[0],float(std::cos(angle)))&&near(view[2],float(-std::sin(angle))),"inverse yaw in view");
    float projection[16]={1,0,0,0,0,2,0,0,0,0,-1,-.2f,0,0,-1,0};auto tag=camera.match(view,projection,pose.observed_ns);
    check(tag.valid&&near(tag.fov[0],float(-std::atan(1.0)))&&near(tag.fov[2],float(std::atan(.5))),"matched source FOV and pose");
    check(!camera.match(view,projection,pose.observed_ns+300000000).valid,"stale camera association rejected");
    float saved[12];std::memcpy(saved,view,sizeof view);auto bad=pose;bad.q[0]=std::numeric_limits<float>::quiet_NaN();
    check(!camera.apply(7,view,bad,pose.observed_ns)&&std::memcmp(saved,view,sizeof view)==0,"invalid pose leaves source unchanged");
    bad=pose;bad.p[0]=2;check(!camera.apply(7,view,bad,pose.observed_ns)&&std::memcmp(saved,view,sizeof view)==0,"motion bound leaves source unchanged");
    check(!camera.apply(7,view,pose,pose.observed_ns+300000000),"stale pose rejected");
    pose.generation=2;std::memcpy(view,base,sizeof view);check(camera.apply(7,view,pose,pose.observed_ns)&&std::memcmp(view,base,sizeof view)==0,"origin generation resets anchor");
    // Pitch and roll survive; a new game camera pose remains the body anchor.
    pose.q[1]=0;pose.q[0]=float(std::sin(.1));pose.q[2]=float(std::sin(.1));pose.q[3]=float(std::sqrt(1-2*std::sin(.1)*std::sin(.1)));
    pose.observed_ns+=10000000;check(camera.apply(7,view,pose,pose.observed_ns),"pitch and roll");
    check(std::abs(view[1])+std::abs(view[6])>.01,"rotation axes retained");
    float moved[12];std::memcpy(moved,base,sizeof moved);moved[3]=-20;pose.generation=3;
    check(camera.apply(9,moved,pose,pose.observed_ns)&&near(moved[3],-20),"new gameplay camera anchors independently");
    std::cout<<"Native mono head camera validates poses, rotation, translation, source FOV, reanchor and nonaccumulation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
