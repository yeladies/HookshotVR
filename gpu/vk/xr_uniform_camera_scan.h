// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_camera_submission.h"
#include <algorithm>
#include <vector>
namespace mkwvk {
struct UniformCameraScan {
    bool eligible=false,truncated=false;
    uint32_t ambiguous_offsets=0;
    std::vector<uint32_t> view_offsets,projection_offsets,vp_offsets;
};
// Read-only candidate search at 16-byte-aligned offsets in one bounded copied UBO.
// Coincident values, especially projection alone, do not prove field semantics.
inline UniformCameraScan scan_submitted_uniform(const SceneSubmissionSnapshot* snapshot,int64_t now,
                                                const float* data,size_t count) {
    UniformCameraScan out;
    if(!snapshot||!data||count>65536/4||count<12||snapshot->captured_ns<=0||now<snapshot->captured_ns||
       now-snapshot->captured_ns>500000000)return out;
    struct Camera {uint64_t id;const float* view;const float* projection;float vp[16]{};};
    std::vector<Camera> cameras;
    for(size_t i=0;i<snapshot->count&&i<snapshot->cameras.size();++i){
        const auto& c=snapshot->cameras[i];const auto& p=c.pair;
        if(!p.revision||!p.primary.camera_id||c.received_ns<=0||c.received_ns>snapshot->captured_ns||
           snapshot->captured_ns-c.received_ns>250000000||!camera_view_valid(p.primary.view)||
           !camera_projection_shape(p.uniforms.projection))continue;
        Camera camera{p.primary.camera_id,p.primary.view,p.uniforms.projection,{}};
        for(int r=0;r<4;++r)for(int col=0;col<4;++col){double v=0;for(int k=0;k<4;++k)
            v+=double(camera.projection[4*r+k])*(k==3?(col==3?1:0):camera.view[4*k+col]);camera.vp[4*r+col]=float(v);}
        bool finite=true;for(float value:camera.vp)finite=finite&&std::isfinite(value);
        if(finite)cameras.push_back(camera);
    }
    if(cameras.empty())return out;out.eligible=true;
    size_t hits=0;
    for(size_t offset=0;offset+12<=count;offset+=4){
        unsigned kinds=0;uint64_t identity=0;bool ambiguous=false;
        for(const auto& camera:cameras){
            bool view=true,projection=offset+16<=count,vp=projection;
            for(int i=0;i<12;++i)if(!std::isfinite(data[offset+i])||
                std::abs(double(data[offset+i])-camera.view[i])>(i%4==3?.01:.0001))view=false;
            if(offset+16<=count)for(int i=0;i<16;++i){
                const double value=data[offset+i];
                if(!std::isfinite(value)||std::abs(value-camera.projection[i])>((i==2||i==6)?.01:.0001))projection=false;
                if(!std::isfinite(value)||std::abs(value-camera.vp[i])>.02+std::abs(double(camera.vp[i]))*.0001)vp=false;
            }
            unsigned matched=(view?1u:0u)|(projection?2u:0u)|(vp?4u:0u);
            if(matched){if(identity&&identity!=camera.id)ambiguous=true;identity=camera.id;kinds|=matched;}
        }
        if(!kinds)continue;
        if(ambiguous){++out.ambiguous_offsets;continue;}
        if(hits++>=64){out.truncated=true;break;}
        if(kinds&1)out.view_offsets.push_back(uint32_t(offset*4));
        if(kinds&2)out.projection_offsets.push_back(uint32_t(offset*4));
        if(kinds&4)out.vp_offsets.push_back(uint32_t(offset*4));
    }
    return out;
}
}
