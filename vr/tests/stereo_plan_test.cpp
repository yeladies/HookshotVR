// SPDX-License-Identifier: GPL-3.0-or-later
#include "stereo_plan.h"
#include <stdexcept>
#include <iostream>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
double horizontal(const ProjectionMatrix& matrix){return (matrix[2]*-2+matrix[3])/(matrix[14]*-2+matrix[15]);}
int main(){try {
    GameCameraStamp game{10,20,30,1000000000};
    EyeFrameInput frame{};frame.sequence=40;frame.origin_generation=50;frame.observed_ns=1000000000;
    frame.predicted_display_time=2000000000;frame.should_render=true;
    for(int i=0;i<2;++i){frame.eyes[i].pose={{0,0,0,1},{i?0.032f:-0.032f,0,0}};frame.eyes[i].fov={-0.7f,0.7f,0.8f,-0.8f};
        frame.flags[i]=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;}
    StereoCalibration calibration{};calibration.camera_id=10;calibration.origin_generation=50;
    calibration.near_z=0.05;calibration.far_z=1000;
    DiagnosticStereoPlan plan{};
    check(build_stereo_plan(game,frame,calibration,1010000000,plan),"coherent plan");
    check(plan.game.revision==20&&plan.xr_sequence==40&&plan.predicted_display_time==2000000000,"provenance retained");
    check(plan.eyes[1].view.eye[0]-plan.eyes[0].view.eye[0]>0.0639&&horizontal(plan.eyes[0].world_to_clip)>horizontal(plan.eyes[1].world_to_clip),"IPD and composed stereo parallax");
    frame.flags[1]=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    check(!build_stereo_plan(game,frame,calibration,1010000000,plan)&&plan.xr_sequence==40,"one invalid eye cannot partially publish");
    frame.flags[1]=frame.flags[0];frame.origin_generation=51;
    check(!build_stereo_plan(game,frame,calibration,1010000000,plan),"recenter requires new calibration");
    frame.origin_generation=50;calibration.camera_id=11;
    check(!build_stereo_plan(game,frame,calibration,1010000000,plan),"camera transition invalidates calibration");
    calibration.camera_id=10;
    check(!build_stereo_plan(game,frame,calibration,1200000000,plan),"stale game/XR rejected");
    check(!build_stereo_plan(game,frame,calibration,900000000,plan),"future host stamps rejected");
    frame.observed_ns=1060000000;
    check(!build_stereo_plan(game,frame,calibration,1070000000,plan),"widely separated observations rejected");
    frame.observed_ns=game.received_ns;frame.should_render=false;
    check(!build_stereo_plan(game,frame,calibration,1010000000,plan),"no plan when runtime should not render");
    frame.should_render=true;calibration.near_z=0;
    check(!build_stereo_plan(game,frame,calibration,1010000000,plan),"no invented projection calibration");
    std::cout<<"Unsubmitted stereo-plan coherence checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}