// SPDX-License-Identifier: GPL-3.0-or-later
#include "camera_math.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace hookshot::vr;
static void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static bool near(double a, double b) { return std::abs(a-b)<1e-5; }
int main() {
    try {
        constexpr auto valid = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        TrackingOrigin origin{};
        XrPosef pose{{0,0,0,1},{1,2,3}};
        CameraView camera{};
        check(tracked_camera_view(origin,pose,valid,camera),"identity pose accepted");
        for (int r=0;r<3;++r) {
            double at_eye=camera.rows[r][3];
            for (int c=0;c<3;++c) at_eye+=camera.rows[r][c]*camera.eye[c];
            check(near(at_eye,0),"eye maps to zero");
            check(near(camera.rows[r][r],1),"identity basis");
        }
        origin.world_position[0]=100000;
        origin.local_reference=pose.position;
        origin.world_units_per_metre=2;
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.eye[0],100000)&&near(camera.eye[1],0),"recenter at world anchor");
        pose.position.x+=0.25f;
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.eye[0],100000.5),"metres mapped once");
        origin.yaw_radians=std::acos(-1.0)/2;
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.eye[2],-0.5)&&near(camera.rows[2][0],1),"body yaw rotates offset and basis");
        origin={};pose={{0.70710678f,0,0,0.70710678f},{0,0,0}};
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.rows[2][1],-1)&&near(camera.rows[1][2],1),"head pitch retained");
        pose={{0,0.70710678f,0,0.70710678f},{0,0,0}};
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.rows[2][0],1)&&near(camera.rows[0][2],-1),"head yaw retained");
        origin={};pose={{0,0,0.70710678f,0.70710678f},{0,0,0}};
        check(tracked_camera_view(origin,pose,valid,camera)&&near(camera.rows[0][1],1)&&near(camera.rows[1][0],-1),"roll retained");
        pose={{0,0,0,1},{-0.032f,1.6f,0}};
        CameraView left{},right{};
        check(tracked_camera_view(origin,pose,valid,left),"left eye");
        pose.position.x=0.032f;
        check(tracked_camera_view(origin,pose,valid,right)&&near(right.eye[0]-left.eye[0],0.064),"runtime eye separation preserved");
        // Same point in front of both eyes has opposing horizontal eye coordinates.
        check(left.rows[0][3]>0&&right.rows[0][3]<0,"stereo parallax direction");
        const double saved=camera.eye[0];
        check(!tracked_camera_view(origin,pose,XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,camera),"missing position rejected");
        pose.orientation.w=0;
        check(!tracked_camera_view(origin,pose,valid,camera)&&camera.eye[0]==saved,"invalid quaternion leaves output untouched");
        pose.orientation.w=1;pose.position.x=std::numeric_limits<float>::quiet_NaN();
        check(!tracked_camera_view(origin,pose,valid,camera),"nonfinite position rejected");
        pose.position.x=0;origin.world_units_per_metre=0;
        check(!tracked_camera_view(origin,pose,valid,camera),"zero scale rejected");
        std::cout<<"Tracked camera coordinate checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}