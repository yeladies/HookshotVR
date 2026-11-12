// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <limits>
namespace hookshot::vr {
struct FovAngles { double left,right,up,down; };
enum class ClipDepth { MinusOneToOne, ZeroToOne };
enum class DepthOrder { Forward, Reversed };
using ProjectionMatrix=std::array<double,16>;
// RH, -Z forward, row-major, column-vector multiplication. Y flip is an explicit caller choice.
inline bool eye_projection(FovAngles fov,double near_z,double far_z,ClipDepth clip,DepthOrder order,
                           bool flip_y,ProjectionMatrix& out) {
    const double angles[]={fov.left,fov.right,fov.up,fov.down};
    constexpr double half_pi=1.57079632679489661923;
    for(double a:angles)if(!std::isfinite(a)||a<=-half_pi||a>=half_pi)return false;
    if(!std::isfinite(near_z)||near_z<=0||std::isnan(far_z)||far_z<=near_z)return false;
    if(clip!=ClipDepth::MinusOneToOne&&clip!=ClipDepth::ZeroToOne)return false;
    if(order!=DepthOrder::Forward&&order!=DepthOrder::Reversed)return false;
    const double l=std::tan(fov.left),r=std::tan(fov.right),u=std::tan(fov.up),d=std::tan(fov.down);
    if(r-l<=1e-8||u-d<=1e-8)return false;
    const double near_ndc=order==DepthOrder::Reversed?1:(clip==ClipDepth::MinusOneToOne?-1:0);
    const double far_ndc=order==DepthOrder::Forward?1:(clip==ClipDepth::MinusOneToOne?-1:0);
    ProjectionMatrix p{};
    p[0]=2/(r-l);p[2]=(r+l)/(r-l);
    p[5]=2/(u-d);p[6]=(u+d)/(u-d);
    if(flip_y){p[5]=-p[5];p[6]=-p[6];}
    if(std::isinf(far_z)) {p[10]=-far_ndc;p[11]=near_z*(near_ndc-far_ndc);}
    else {
        p[10]=(near_ndc*near_z-far_ndc*far_z)/(far_z-near_z);
        p[11]=near_z*(near_ndc+p[10]);
    }
    p[14]=-1;
    for(double value:p)if(!std::isfinite(value))return false;
    out=p;return true;
}
} // namespace hookshot::vr