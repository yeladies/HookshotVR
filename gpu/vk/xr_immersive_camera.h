// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace mkwvk {
struct ImmersiveTag {
    bool valid=false,menu=false,temporal_stereo=false;uint32_t eye=0;uint64_t generation=0;int64_t observed_ns=0;
    float orientation[4]{0,0,0,1},position[3]{},fov[4]{};
};
struct ImmersivePose {float q[4]{0,0,0,1},p[3]{};bool temporal_stereo=false;uint32_t eye=0;uint64_t generation=0;int64_t observed_ns=0;};
// Bounded head/eye camera math. Temporal stereo uses distinct game presents.
// No guest pointers or XR API are accessed by this helper.
class ImmersiveCamera {
    uint64_t camera_=0,generation_=0;float reference_[9]{},origin_[3]{},base_[12]{},last_[12]{};
    bool anchored_=false;struct History {float view[12]{};ImmersivePose pose;bool valid=false;};
    std::array<History,64> history_{};unsigned cursor_=0;
    static bool rotation(const float* q,float* r){
        double n=0;for(int i=0;i<4;++i){if(!std::isfinite(q[i]))return false;n+=double(q[i])*q[i];}
        if(std::abs(n-1)>0.01)return false;
        const double x=q[0]/std::sqrt(n),y=q[1]/std::sqrt(n),z=q[2]/std::sqrt(n),w=q[3]/std::sqrt(n);
        const double a[9]={1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)};
        for(int i=0;i<9;++i)r[i]=float(a[i]);return true;
    }
    static bool rigid(const float* v){
        for(int i=0;i<12;++i)if(!std::isfinite(v[i]))return false;
        for(int r=0;r<3;++r)for(int s=0;s<3;++s){double dot=0;for(int c=0;c<3;++c)dot+=double(v[4*r+c])*v[4*s+c];if(std::abs(dot-(r==s))>0.002)return false;}
        const double d=v[0]*(double(v[5])*v[10]-double(v[6])*v[9])-v[1]*(double(v[4])*v[10]-double(v[6])*v[8])+v[2]*(double(v[4])*v[9]-double(v[5])*v[8]);
        return std::abs(d-1)<0.003;
    }
public:
    bool apply(uint64_t camera,float* view,const ImmersivePose& pose,int64_t now,const ImmersivePose* anchor=nullptr){
        float current[9];if(!camera||!pose.generation||pose.observed_ns<=0||now<pose.observed_ns||now-pose.observed_ns>250000000||!rigid(view)||!rotation(pose.q,current))return false;
        for(float x:pose.p)if(!std::isfinite(x))return false;
        if(pose.temporal_stereo&&pose.eye>1)return false;
        float reference_rotation[9];const auto& reference=anchor?*anchor:pose;
        if(reference.generation!=pose.generation||reference.observed_ns<=0||now<reference.observed_ns||now-reference.observed_ns>250000000||!rotation(reference.q,reference_rotation))return false;
        for(float x:reference.p)if(!std::isfinite(x))return false;
        const bool reset=!anchored_||camera_!=camera||generation_!=pose.generation;
        if(reset){camera_=camera;generation_=pose.generation;std::memcpy(reference_,reference_rotation,sizeof reference_rotation);std::memcpy(origin_,reference.p,sizeof origin_);anchored_=true;for(auto& h:history_)h.valid=false;}
        // The engine may revisit the same view without rebuilding it. Never feed
        // a previously overridden matrix back into the relative transformation.
        if(reset||std::memcmp(view,last_,sizeof last_)!=0)std::memcpy(base_,view,sizeof base_);
        float delta[9]{},translation[3]{};
        for(int r=0;r<3;++r){for(int k=0;k<3;++k)translation[r]+=reference_[3*k+r]*(pose.p[k]-origin_[k]);
            for(int c=0;c<3;++c)for(int k=0;k<3;++k)delta[3*r+c]+=reference_[3*k+r]*current[3*k+c];}
        double distance=0;for(float x:translation)distance+=double(x)*x;
        if(distance>1.0)return false; // First test: at most one metre from the anchor.
        float out[12]{};for(int r=0;r<3;++r)for(int c=0;c<4;++c)for(int k=0;k<3;++k)
            out[4*r+c]+=delta[3*k+r]*(base_[4*k+c]-(c==3?translation[k]:0));
        if(!rigid(out))return false;
        std::memcpy(view,out,sizeof out);std::memcpy(last_,out,sizeof out);
        auto& h=history_[cursor_++%history_.size()];std::memcpy(h.view,out,sizeof out);h.pose=pose;h.valid=true;return true;
    }
    ImmersiveTag match(const float* view,const float* projection,int64_t now)const{
        ImmersiveTag out;if(!rigid(view))return out;
        for(int i=0;i<16;++i)if(!std::isfinite(projection[i]))return out;
        if(projection[0]<=0||std::abs(projection[5])<0.000001||std::abs(projection[14]+1)>0.000001)return out;
        for(int i:{1,3,4,7,8,9,12,13,15})if(std::abs(projection[i])>0.000001)return out;
        for(unsigned n=0;n<history_.size();++n){const auto& h=history_[(cursor_-1-n)%history_.size()];
            if(!h.valid||h.pose.generation!=generation_||now<h.pose.observed_ns||now-h.pose.observed_ns>250000000)continue;
            bool same=true;for(int i=0;i<12;++i)same&=std::abs(double(h.view[i])-view[i])<0.0001;if(!same)continue;
            out.valid=true;out.temporal_stereo=h.pose.temporal_stereo;out.eye=h.pose.eye;out.generation=h.pose.generation;out.observed_ns=h.pose.observed_ns;std::memcpy(out.orientation,h.pose.q,sizeof out.orientation);std::memcpy(out.position,h.pose.p,sizeof out.position);
            out.fov[0]=std::atan((projection[2]-1)/projection[0]);out.fov[1]=std::atan((projection[2]+1)/projection[0]);
            out.fov[2]=std::atan((projection[6]+1)/std::abs(projection[5]));out.fov[3]=std::atan((projection[6]-1)/std::abs(projection[5]));return out;
        }
        return out;
    }
};
inline bool immersive_lens_from_fovs(const float left[4],const float right[4],float& vertical,float& aspect){
    double horizontal=0,height=0;
    for(const auto* eye:{left,right}){
        if(!eye)return false;
        for(int i=0;i<4;++i)if(!std::isfinite(eye[i])||std::abs(eye[i])>=1.45)return false;
        if(eye[0]>=0||eye[1]<=0||eye[2]<=0||eye[3]>=0)return false;
        horizontal=std::max(horizontal,std::max(-std::tan(double(eye[0])),std::tan(double(eye[1]))));
        height=std::max(height,std::max(std::tan(double(eye[2])),-std::tan(double(eye[3]))));
    }
    const double fov=2*std::atan(height),ratio=horizontal/height;
    if(!std::isfinite(ratio)||fov<.2||fov>2.8||ratio<.5||ratio>2.5)return false;
    vertical=float(fov);aspect=float(ratio);return true;
}

}
