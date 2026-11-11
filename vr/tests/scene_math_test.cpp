// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene_math.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hookshot::vr;
void require(bool x, const char* message) { if(!x) throw std::runtime_error(message); }
int main() {
    try {
        XrPosef eye{{0,0,0,1},{0,0,0}};
        const XrFovf fov{-0.78539816f,0.78539816f,0.78539816f,-0.78539816f};
        MarkerRect a{}, b{};
        require(marker_rect(eye,fov,{0,0,-2},0.05f,1000,1000,a),"front marker visible");
        require(a.x<500 && a.x+a.width>500 && a.y<500 && a.y+a.height>500,"centered projection");
        eye.position.x=-0.032f; marker_rect(eye,fov,{0,0,-2},0.05f,1000,1000,a);
        eye.position.x=0.032f; marker_rect(eye,fov,{0,0,-2},0.05f,1000,1000,b);
        require(a.x>b.x,"stereo disparity direction");
        eye.position={0,0,0}; marker_rect(eye,fov,{0,0,-1},0.05f,1000,1000,a);
        marker_rect(eye,fov,{0,0,-2},0.05f,1000,1000,b);
        require(a.width>b.width,"distance changes apparent size");
        require(!marker_rect(eye,fov,{0,0,1},0.05f,1000,1000,a),"behind eye rejected");
        require(!marker_rect(eye,fov,{0,0,-0.01f},0.05f,1000,1000,a),"near singularity rejected");
        eye.orientation={0,0,0.70710678f,0.70710678f};
        require(marker_rect(eye,fov,{0.5f,0,-2},0.05f,1000,1000,a) && a.y>500,"head roll respected");
        eye.orientation={0,0,0,1};
        require(marker_rect(eye,fov,{0,0.5f,-2},0.05f,1000,1000,a) && a.y<500,"framebuffer Y convention");
        require(!marker_rect(eye,fov,{10,0,-2},0.05f,1000,1000,a),"outside FOV rejected");
        const XrFovf asymmetric{-0.5f,0.9f,0.7f,-0.6f};
        require(marker_rect(eye,asymmetric,{0,0,-2},0.05f,1000,1000,a) && a.x<400,"asymmetric runtime FOV used");
        require(marker_rect(eye,fov,{1.99f,0,-2},0.05f,1000,1000,a) && a.x+a.width<=1000,"edge clipped safely");
        eye.position.x=std::numeric_limits<float>::quiet_NaN();
        require(!marker_rect(eye,fov,{0,0,-2},0.05f,1000,1000,a),"invalid position rejected");
        require(!pose_valid(XR_SPACE_LOCATION_ORIENTATION_VALID_BIT),"orientation alone is not 6DoF");
        require(pose_valid(XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT),"valid pose accepted");
        std::cout<<"Scene projection and pose validity checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
