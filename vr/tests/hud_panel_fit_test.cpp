// SPDX-License-Identifier: GPL-3.0-or-later
#include "hud_panel_fit.h"
#include "camera_math.h"
#include <stdexcept>
#include <iostream>
#include <limits>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    std::array<XrView,2> eyes{};for(unsigned i=0;i<2;++i){eyes[i].pose.orientation.w=1;eyes[i].pose.position.x=i?.032f:-.032f;eyes[i].fov={-.8f,.8f,.8f,-.8f};}
    const auto flags=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
    HudPanelFit out;auto fit=[&](double aspect=16./9){return fit_hud_panel(eyes,flags,aspect,1.5,.7,.8,out);};
    check(fit()&&std::abs(out.width/out.height-16./9)<.00001&&std::abs(out.height-3*std::tan(.35))<.00001,"preserve full UI aspect and comfortable angular cap");
    const auto full=out;eyes[1].fov.angleLeft=-.2f;check(fit()&&out.width<full.width,"narrower eye limits both views");
    // Independent identity-pose corner check, including IPD and an asymmetric lens.
    for(const auto& e:eyes)for(int sx:{-1,1})for(int sy:{-1,1}){
        const double x=(sx*out.width/2-e.pose.position.x)/out.distance,y=(sy*out.height/2-e.pose.position.y)/out.distance;
        check(x>=std::tan(e.fov.angleLeft)*.8-1e-6&&x<=std::tan(e.fov.angleRight)*.8+1e-6&&y>=std::tan(e.fov.angleDown)*.8-1e-6&&y<=std::tan(e.fov.angleUp)*.8+1e-6,"all original UI corners inside each safe frustum");
    }
    eyes[1].pose.orientation.y=.1f;eyes[1].pose.orientation.w=std::sqrt(.99f);check(fit(),"canted eye is handled by inverse pose rotation");
    for(const auto& e:eyes){CameraView view;TrackingOrigin origin;
        check(tracked_camera_view(origin,e.pose,XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,view),"independent tracked view");
        for(int sx:{-1,1})for(int sy:{-1,1}){const double p[4]={sx*out.width/2,sy*out.height/2,-out.distance,1};double v[3]{};
            for(int row=0;row<3;++row)for(int col=0;col<4;++col)v[row]+=view.rows[row][col]*p[col];
            check(-v[2]>0&&v[0]/-v[2]>=std::tan(e.fov.angleLeft)*.8-1e-6&&v[0]/-v[2]<=std::tan(e.fov.angleRight)*.8+1e-6&&v[1]/-v[2]>=std::tan(e.fov.angleDown)*.8-1e-6&&v[1]/-v[2]<=std::tan(e.fov.angleUp)*.8+1e-6,"canted view corners verified with existing camera transform");
        }
    }
    const auto saved=out;eyes[0].pose.orientation.w=0;
    check(!fit()&&out.width==saved.width&&out.height==saved.height,"invalid pose cannot change panel");
    eyes[0].pose.orientation.w=1;eyes[0].fov.angleLeft=0;check(!fit(),"invalid lens rejected");eyes[0].fov.angleLeft=-.8f;
    check(!fit_hud_panel(eyes,0,16./9,1.5,.7,.8,out),"untracked views rejected");
    check(!fit_hud_panel(eyes,flags,std::numeric_limits<double>::quiet_NaN(),1.5,.7,.8,out),"invalid aspect rejected");
    eyes[1].pose.orientation={0,1,0,0};check(!fit(),"panel behind an eye is never accepted");
    std::cout<<"HUD panel preserves aspect and fits both actual eye frusta without changing world FOV\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
