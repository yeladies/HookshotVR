// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "projection_math.h"
#include <cstddef>
namespace hookshot::vr {
struct ProjectionDepthRange {
    double near_z=0,far_z=0;DepthOrder order=DepthOrder::Forward;
    // A float coefficient rounded to +/-1 cannot distinguish a very distant
    // finite far plane from an infinite one. Never claim a recovered finite far.
    bool far_infinite_or_unresolved=false;
};
// Only for the independently reviewed NVN -1..1 path. Matrix coefficients alone
// cannot identify the clip convention; do not apply to arbitrary Vulkan matrices.
inline bool nvn_projection_depth(const float* p,size_t count,ClipDepth reviewed_clip,
                                  ProjectionDepthRange& out) {
    if(!p||count<16||reviewed_clip!=ClipDepth::MinusOneToOne)return false;
    for(size_t i=0;i<16;++i)if(!std::isfinite(p[i]))return false;
    for(int i:{1,3,4,7,8,9,12,13,15})if(std::abs(p[i])>1e-6)return false;
    if(p[0]<=0||std::abs(p[5])<=1e-6||std::abs(p[14]+1)>1e-6)return false;
    const double a=p[10],b=p[11];
    if(std::abs(a)<1||b==0||a*b<=0)return false;
    ProjectionDepthRange candidate;candidate.order=a<0?DepthOrder::Forward:DepthOrder::Reversed;
    const double near_ndc=a<0?-1:1,far_ndc=-near_ndc;
    candidate.near_z=b/(a+near_ndc);
    const double denominator=a+far_ndc;
    candidate.far_infinite_or_unresolved=denominator==0;
    candidate.far_z=denominator==0?std::numeric_limits<double>::infinity():b/denominator;
    if(!std::isfinite(candidate.near_z)||candidate.near_z<=0||std::isnan(candidate.far_z)||candidate.far_z<=candidate.near_z)return false;
    out=candidate;return true;
}
}
