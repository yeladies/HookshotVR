// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vk_api.h"
#include <cmath>
#include <cstdint>
namespace mkwvk {
inline bool camera_view_valid(const float* rows) {
    for(int i=0;i<12;++i)if(!std::isfinite(rows[i]))return false;
    for(int r=0;r<3;++r)for(int s=0;s<3;++s) {
        double dot=0;for(int c=0;c<3;++c)dot+=double(rows[4*r+c])*rows[4*s+c];
        if(std::abs(dot-(r==s?1:0))>0.002)return false;
    }
    const double det=rows[0]*(double(rows[5])*rows[10]-double(rows[6])*rows[9])-
        rows[1]*(double(rows[4])*rows[10]-double(rows[6])*rows[8])+rows[2]*(double(rows[4])*rows[9]-double(rows[5])*rows[8]);
    return std::abs(det-1)<0.003;
}
inline bool camera_matrix_matches(const float* a,const float* b) {
    for(int i=0;i<12;++i)if(std::abs(double(a[i])-b[i])>(i%4==3?0.01:0.0001))return false;
    return true;
}
inline bool camera_projection_shape(const float* p) {
    for(int i=0;i<16;++i)if(!std::isfinite(p[i]))return false;
    const int zero[]={1,3,4,7,8,9,12,13,15};
    for(int i:zero)if(std::abs(p[i])>0.000001f)return false;
    return p[0]>0&&std::abs(p[5])>0.000001f&&std::abs(p[14]+1)<0.000001f;
}
struct CameraCapturePair {
    MkwVkCameraObservation primary{},uniforms{};
    uint64_t revision=0;
};
struct CameraCaptureStats { uint64_t primary=0,uniforms=0,matches=0,rejected=0; };
// Single-owner state; caller supplies synchronization and a monotonic receiving clock.
class CameraCapture {
    MkwVkCameraObservation primary_{};
    int64_t primary_at_=0,pair_at_=0;
    CameraCapturePair pair_{};
    CameraCaptureStats stats_{};
    uint64_t last_sequence_=0;
public:
    bool observe(const MkwVkCameraObservation& sample,int64_t received_ns) {
        // A new primary observation invalidates an old association even if the new view is unusable/debug.
        if(sample.flags&MKWVK_CAMERA_PRIMARY) {primary_={};primary_at_=0;pair_={};pair_at_=0;}
        if(sample.size!=sizeof sample||!sample.camera_id||!sample.sequence||sample.game_present<0||received_ns<=0||sample.sequence<=last_sequence_||
           !camera_view_valid(sample.view)|| (sample.flags!=MKWVK_CAMERA_PRIMARY&&sample.flags!=MKWVK_CAMERA_UNIFORM)) {
            ++stats_.rejected;return false;
        }
        last_sequence_=sample.sequence;
        if(sample.flags==MKWVK_CAMERA_PRIMARY) {
            for(double value:sample.eye)if(!std::isfinite(value)){++stats_.rejected;return false;}
            primary_=sample;primary_at_=received_ns;++stats_.primary;
            pair_={};pair_at_=0;return true;
        }
        if(!camera_projection_shape(sample.projection)){++stats_.rejected;return false;}
        ++stats_.uniforms;
        if(!primary_at_||received_ns<primary_at_||received_ns-primary_at_>100000000||
           sample.game_present!=primary_.game_present||!camera_matrix_matches(sample.view,primary_.view))return true;
        const auto revision=++stats_.matches;
        pair_={primary_,sample,revision};pair_at_=received_ns;return true;
    }
    bool latest(int64_t now_ns,CameraCapturePair& out) const {
        if(!pair_at_||now_ns<pair_at_||now_ns-pair_at_>100000000){out={};return false;}
        out=pair_;return true;
    }
    CameraCaptureStats stats() const {return stats_;}
};
void xr_camera_observation(const MkwVkCameraObservation* sample);
bool xr_camera_latest(CameraCapturePair* out);
}