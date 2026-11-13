// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include "depth_scene_renderer.h"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
 XrPosef anchor{},left{},right{};anchor.orientation.w=left.orientation.w=right.orientation.w=1;left.position.x=-.032f;right.position.x=.032f;XrFovf fov{-.6f,.6f,.6f,-.6f};
 auto l=hookshot::vr::depth_scene_mvp(left,anchor,fov),r=hookshot::vr::depth_scene_mvp(right,anchor,fov);
 check(l[3]>0&&r[3]<0&&std::abs(l[3]+r[3])<1e-6,"opposite correct eye disparity");
 check(l[5]<0&&l[14]==-1,"Vulkan Y-down and negative-Z forward");
 XrPosef head=anchor;head.position.x=.2f;auto moved=hookshot::vr::depth_scene_mvp(head,anchor,fov);check(moved[3]<0,"scene moves opposite physical head translation");
 head.orientation={0,std::sqrt(.5f),0,std::sqrt(.5f)};head.position={0,0,0};auto turned=hookshot::vr::depth_scene_mvp(head,anchor,fov);
 const float x=-5;check(std::abs(turned[0]*x+turned[3])<1e-5&&turned[12]*x+turned[15]>0,"yaw turns viewing direction in room space");
 auto stable=hookshot::vr::depth_scene_mvp(head,head,fov);check(std::abs(stable[1])<1e-5&&std::abs(stable[2])<1e-5&&stable[0]>0,"first head yaw anchors scene without following later motion");
 std::cout<<"Depth scene stereo disparity, room anchor, Y convention and head translation verified\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
