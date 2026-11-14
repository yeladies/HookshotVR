// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openxr/openxr.h>
#include <array>
#include <cmath>
namespace hookshot::vr {
struct HudPanelFit {float width=0,height=0,distance=0;};
// Fit the entire original UI rectangle, preserving aspect, in both eye frusta.
// Views MUST be expressed relative to the head-centred panel space. This helper
// neither extracts a HUD nor changes the world lens or submitted render poses.
inline bool fit_hud_panel(const std::array<XrView,2>& eyes,XrViewStateFlags flags,double aspect,
        double distance,double max_vertical_angle,double tangent_fraction,HudPanelFit& output){
    const auto valid=XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT;
    if((flags&valid)!=valid||!std::isfinite(aspect)||aspect<.5||aspect>4||!std::isfinite(distance)||distance<.5||distance>5||
       !std::isfinite(max_vertical_angle)||max_vertical_angle<.1||max_vertical_angle>1.4||
       !std::isfinite(tangent_fraction)||tangent_fraction<.5||tangent_fraction>.95)return false;
    double rotation[2][3][3]{},bounds[2][4]{};
    for(unsigned eye=0;eye<2;++eye){const auto& v=eyes[eye];const auto q=v.pose.orientation;
        const double n=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
        if(!std::isfinite(n)||std::abs(n-1)>.001||!std::isfinite(v.pose.position.x)||!std::isfinite(v.pose.position.y)||!std::isfinite(v.pose.position.z))return false;
        const double inv=1/std::sqrt(n),x=q.x*inv,y=q.y*inv,z=q.z*inv,w=q.w*inv;
        const double r[3][3]={{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
                             {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
                             {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}};
        for(int a=0;a<3;++a)for(int b=0;b<3;++b)rotation[eye][a][b]=r[a][b];
        const double angles[4]={v.fov.angleLeft,v.fov.angleRight,v.fov.angleDown,v.fov.angleUp};
        for(int i=0;i<4;++i){if(!std::isfinite(angles[i])||std::abs(angles[i])>=1.45||(i%2?angles[i]<=0:angles[i]>=0))return false;bounds[eye][i]=std::tan(angles[i])*tangent_fraction;}
    }
    auto fits=[&](double half_height){for(unsigned eye=0;eye<2;++eye)for(int sx:{-1,1})for(int sy:{-1,1}){
        const auto p=eyes[eye].pose.position;const double corner[3]={sx*half_height*aspect-p.x,sy*half_height-p.y,-distance-p.z};double local[3]{};
        for(int a=0;a<3;++a)for(int b=0;b<3;++b)local[a]+=rotation[eye][b][a]*corner[b];
        const double depth=-local[2];if(depth<=.001||local[0]<bounds[eye][0]*depth||local[0]>bounds[eye][1]*depth||local[1]<bounds[eye][2]*depth||local[1]>bounds[eye][3]*depth)return false;
    }return true;};
    if(!fits(0))return false;
    double low=0,high=distance*std::tan(max_vertical_angle/2);
    for(int i=0;i<48;++i){const double mid=(low+high)/2;if(fits(mid))low=mid;else high=mid;}
    if(low<.001)return false;
    low*=1-1e-6;
    output={float(2*low*aspect),float(2*low),float(distance)};return true;
}
}
