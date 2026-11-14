/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HOOKSHOT_VR_VIEW_PARAMETERS_H
#define HOOKSHOT_VR_VIEW_PARAMETERS_H
#include <math.h>
#include <string.h>
typedef struct HostVrViewParameters { double eye[3],target[3]; float cached[9]; } HostVrViewParameters;
static inline int host_vr_rigid_view(const float* v) {
    for(int i=0;i<12;++i)if(!isfinite(v[i]))return 0;
    for(int r=0;r<3;++r)for(int s=0;s<3;++s){double dot=0;for(int k=0;k<3;++k)dot+=(double)v[4*r+k]*v[4*s+k];if(fabs(dot-(r==s))>.002)return 0;}
    const double d=v[0]*((double)v[5]*v[10]-(double)v[6]*v[9])-v[1]*((double)v[4]*v[10]-(double)v[6]*v[8])+v[2]*((double)v[4]*v[9]-(double)v[5]*v[8]);
    return fabs(d-1)<.003;
}
/* The supported gameplay view rebuilds these derived float eye/target/up caches
   and double eye/target outputs before producing its matrix. Update them with the
   tracked matrix, rather than leaving spatial queries on the body camera.
   This helper owns no guest pointer, and fails atomically on an unknown layout. */
static inline int host_vr_view_parameters(const float original[12],const float tracked[12],const HostVrViewParameters* input,HostVrViewParameters* output) {
    if(!original||!tracked||!input||!output||!host_vr_rigid_view(original)||!host_vr_rigid_view(tracked))return 0;
    double distance=0;
    for(int k=0;k<3;++k){
        if(!isfinite(input->eye[k])||!isfinite(input->target[k]))return 0;
        for(int p=0;p<3;++p)if(!isfinite(input->cached[3*p+k]))return 0;
        if(fabs(input->cached[k]-input->eye[k])>.002+fabs(input->eye[k])*1e-6||fabs(input->cached[3+k]-input->target[k])>.002+fabs(input->target[k])*1e-6)return 0;
        const double delta=input->eye[k]-input->target[k];distance+=delta*delta;
        double eye=0;for(int r=0;r<3;++r)eye-=(double)original[4*r+k]*original[4*r+3];
        if(fabs(eye-input->eye[k])>.005+fabs(eye)*1e-5)return 0;
    }
    distance=sqrt(distance);if(!isfinite(distance)||distance<.0001||distance>1e7)return 0;
    double upnorm=0;for(int k=0;k<3;++k){
        if(fabs((input->eye[k]-input->target[k])/distance-original[8+k])>.002)return 0;
        upnorm+=(double)input->cached[6+k]*input->cached[6+k];
    }
    if(fabs(upnorm-1)>.003)return 0;
    HostVrViewParameters next;
    for(int k=0;k<3;++k){
        double eye=0;for(int r=0;r<3;++r)eye-=(double)tracked[4*r+k]*tracked[4*r+3];
        next.eye[k]=eye;next.target[k]=eye-(double)tracked[8+k]*distance;
        next.cached[k]=(float)eye;next.cached[3+k]=(float)next.target[k];next.cached[6+k]=tracked[4+k];
        if(!isfinite(next.cached[k])||!isfinite(next.cached[3+k]))return 0;
    }
    *output=next;return 1;
}
/* Perspective stores angle, sin(half angle), cos(half angle), tan(half angle),
   then aspect. Consumers must never see a VR tangent with the old angular cache. */
static inline int host_vr_lens_parameters(float tangent,float aspect,float output[5]) {
    if(!output||!isfinite(tangent)||!isfinite(aspect)||tangent<=.2f||tangent>=16||aspect<.5f||aspect>2.5f)return 0;
    const double cosine=1/sqrt(1+(double)tangent*tangent);
    const float next[5]={(float)(2*atan((double)tangent)),(float)(tangent*cosine),(float)cosine,tangent,aspect};
    memcpy(output,next,sizeof next);return 1;
}
#endif
