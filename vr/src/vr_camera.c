/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vr_camera.h"
#include <math.h>
#include <string.h>

static int finite_array(const double* p,unsigned count) {
    for(unsigned i=0;i<count;++i)if(!isfinite(p[i]))return 0;
    return 1;
}
static void identity(double m[16]) {
    memset(m,0,16*sizeof *m);m[0]=m[5]=m[10]=m[15]=1;
}
static int pose_matrix(const VrCameraPose* p,double m[16]) {
    const uint32_t needed=VR_CAMERA_POSITION_VALID|VR_CAMERA_ORIENTATION_VALID;
    if((p->flags&needed)!=needed||!finite_array(p->position,3)||!finite_array(p->orientation,4))return 0;
    const double* q=p->orientation;
    const double norm=q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3];
    if(!isfinite(norm)||fabs(norm-1)>.01)return 0;
    const double k=1/sqrt(norm),x=q[0]*k,y=q[1]*k,z=q[2]*k,w=q[3]*k;
    identity(m);
    m[0]=1-2*(y*y+z*z);m[1]=2*(x*y-z*w);m[2]=2*(x*z+y*w);
    m[4]=2*(x*y+z*w);m[5]=1-2*(x*x+z*z);m[6]=2*(y*z-x*w);
    m[8]=2*(x*z-y*w);m[9]=2*(y*z+x*w);m[10]=1-2*(x*x+y*y);
    for(unsigned i=0;i<3;++i)m[4*i+3]=p->position[i];
    return 1;
}
static int multiply(const double a[16],const double b[16],double out[16]) {
    double next[16]={0};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        for(unsigned k=0;k<4;++k)next[4*r+c]+=a[4*r+k]*b[4*k+c];
    if(!finite_array(next,16))return 0;
    memcpy(out,next,sizeof next);return 1;
}
static int inverse_rigid(const double m[16],double out[16]) {
    identity(out);
    for(unsigned r=0;r<3;++r){
        for(unsigned c=0;c<3;++c)out[4*r+c]=m[4*c+r];
        for(unsigned k=0;k<3;++k)out[4*r+3]-=out[4*r+k]*m[4*k+3];
    }
    return finite_array(out,16);
}
static int world_pose(const VrCameraRig* rig,const double tracking[16],double world[16]) {
    const double cy=cos(rig->yaw_radians),sy=sin(rig->yaw_radians);
    identity(world);
    for(unsigned c=0;c<3;++c){
        world[c]=cy*tracking[c]+sy*tracking[8+c];
        world[4+c]=tracking[4+c];
        world[8+c]=-sy*tracking[c]+cy*tracking[8+c];
    }
    const double x=tracking[3]-rig->tracking_reference_m[0];
    const double y=tracking[7]-rig->tracking_reference_m[1];
    const double z=tracking[11]-rig->tracking_reference_m[2];
    world[3]=rig->position_world[0]+rig->world_units_per_metre*(cy*x+sy*z);
    world[7]=rig->position_world[1]+rig->world_units_per_metre*y;
    world[11]=rig->position_world[2]+rig->world_units_per_metre*(-sy*x+cy*z);
    return finite_array(world,16);
}
static int projection(const VrCameraSample* s,const double fov[4],double p[16]) {
    const double half_pi=1.57079632679489661923;
    for(unsigned i=0;i<4;++i)if(!isfinite(fov[i])||fabs(fov[i])>=half_pi)return 0;
    const double l=tan(fov[0]),r=tan(fov[1]),u=tan(fov[2]),d=tan(fov[3]);
    if(r-l<=1e-8||u-d<=1e-8)return 0;
    const double near_ndc=s->depth==VR_CAMERA_DEPTH_REVERSED?1:(s->clip==VR_CAMERA_NVN_MINUS_ONE_TO_ONE?-1:0);
    const double far_ndc=s->depth==VR_CAMERA_DEPTH_FORWARD?1:(s->clip==VR_CAMERA_NVN_MINUS_ONE_TO_ONE?-1:0);
    memset(p,0,16*sizeof *p);
    p[0]=2/(r-l);p[2]=(r+l)/(r-l);p[5]=2/(u-d);p[6]=(u+d)/(u-d);
    if(s->flip_y){p[5]=-p[5];p[6]=-p[6];}
    if(isinf(s->far_z)){p[10]=-far_ndc;p[11]=s->near_z*(near_ndc-far_ndc);}
    else {
        p[10]=(near_ndc*s->near_z-far_ndc*s->far_z)/(s->far_z-s->near_z);
        p[11]=s->near_z*(near_ndc+p[10]);
    }
    p[14]=-1;
    return finite_array(p,16);
}
static int planes(const VrCameraSample* s,VrCameraEye* eye) {
    const double* m=eye->world_to_clip;
    for(unsigned i=0;i<6;++i){
        double* p=eye->planes[i];
        const unsigned axis=i/2;
        for(unsigned k=0;k<4;++k)p[k]=m[12+k]+(i%2?-1:1)*m[4*axis+k];
    }
    if(s->clip==VR_CAMERA_VULKAN_ZERO_TO_ONE)memcpy(eye->planes[4],m+8,4*sizeof(double));
    if(s->depth==VR_CAMERA_DEPTH_REVERSED){
        double tmp[4];memcpy(tmp,eye->planes[4],sizeof tmp);
        memcpy(eye->planes[4],eye->planes[5],sizeof tmp);memcpy(eye->planes[5],tmp,sizeof tmp);
    }
    for(unsigned i=0;i<6;++i){
        double* p=eye->planes[i];const double n=hypot(hypot(p[0],p[1]),p[2]);
        if(!isfinite(n)||!finite_array(p,4))return 0;
        /* An infinite far plane is the positive constant inequality, not a normal. */
        if(n==0){if(p[3]<0)return 0;eye->plane_enabled[i]=0;continue;}
        for(unsigned k=0;k<4;++k)p[k]/=n;
        if(!finite_array(p,4))return 0;
        eye->plane_enabled[i]=1;
    }
    return 1;
}
VrCameraStatus vr_camera_build_pose(const VrCameraRig* rig,const VrCameraPose* pose,VrCameraTransform* out) {
    if(!rig||!pose||!out)return VR_CAMERA_BAD_ARGUMENT;
    if(!finite_array(rig->position_world,3)||!finite_array(rig->tracking_reference_m,3)||!isfinite(rig->yaw_radians)||
       !isfinite(rig->world_units_per_metre)||rig->world_units_per_metre<=0)return VR_CAMERA_BAD_RIG;
    double local[16];VrCameraTransform next;
    if(!pose_matrix(pose,local))return VR_CAMERA_BAD_POSE;
    if(!world_pose(rig,local,next.world_from_eye)||!inverse_rigid(next.world_from_eye,next.view))return VR_CAMERA_BAD_RIG;
    *out=next;return VR_CAMERA_OK;
}
/* Decode effective clip distances only for metadata/plane validation. The depth
   coefficients themselves are copied unchanged below, never reconstructed. */
static int native_distances(const VrCameraSample* s,const double p[16],double* near_z,double* far_z) {
    if(!finite_array(p,16)||p[0]<=0||p[5]==0||p[14]!=-1||
       (s->clip!=VR_CAMERA_NVN_MINUS_ONE_TO_ONE&&s->clip!=VR_CAMERA_VULKAN_ZERO_TO_ONE)||
       (s->depth!=VR_CAMERA_DEPTH_FORWARD&&s->depth!=VR_CAMERA_DEPTH_REVERSED))return 0;
    const unsigned zero[]={1,3,4,7,8,9,12,13,15};
    for(unsigned i=0;i<sizeof zero/sizeof *zero;++i)if(p[zero[i]]!=0)return 0;
    const double n=s->depth==VR_CAMERA_DEPTH_REVERSED?1:(s->clip==VR_CAMERA_NVN_MINUS_ONE_TO_ONE?-1:0);
    const double f=s->depth==VR_CAMERA_DEPTH_FORWARD?1:(s->clip==VR_CAMERA_NVN_MINUS_ONE_TO_ONE?-1:0);
    if(p[10]+n==0)return 0;
    *near_z=p[11]/(p[10]+n);
    *far_z=p[10]+f==0?INFINITY:p[11]/(p[10]+f);
    return isfinite(*near_z)&&*near_z>0&&!isnan(*far_z)&&*far_z>*near_z;
}
static VrCameraStatus build_frame(const VrCameraRig* rig,const VrCameraSample* s,const double* native,VrCameraFrame* out) {
    if(!rig||!s||!out)return VR_CAMERA_BAD_ARGUMENT;
    VrCameraSample effective;
    if(native){
        effective=*s;
        if(!native_distances(s,native,&effective.near_z,&effective.far_z))return VR_CAMERA_BAD_PROJECTION;
        s=&effective;
    }
    if(!s->sequence||!s->origin_generation||s->origin_generation!=rig->origin_generation||s->predicted_display_time<=0)return VR_CAMERA_BAD_IDENTITY;
    if(!finite_array(rig->position_world,3)||!finite_array(rig->tracking_reference_m,3)||!isfinite(rig->yaw_radians)||
       !isfinite(rig->world_units_per_metre)||rig->world_units_per_metre<=0)return VR_CAMERA_BAD_RIG;
    if((s->clip!=VR_CAMERA_NVN_MINUS_ONE_TO_ONE&&s->clip!=VR_CAMERA_VULKAN_ZERO_TO_ONE)||
       (s->depth!=VR_CAMERA_DEPTH_FORWARD&&s->depth!=VR_CAMERA_DEPTH_REVERSED)||s->flip_y>1||
       !isfinite(s->near_z)||s->near_z<=0||isnan(s->far_z)||s->far_z<=s->near_z)return VR_CAMERA_BAD_PROJECTION;
    VrCameraFrame next;memset(&next,0,sizeof next);
    next.sequence=s->sequence;next.origin_generation=s->origin_generation;next.predicted_display_time=s->predicted_display_time;
    next.world_units_per_metre=rig->world_units_per_metre;next.near_z=s->near_z;next.far_z=s->far_z;
    double head[16],from_head[16];
    if(!pose_matrix(&s->head,head))return VR_CAMERA_BAD_POSE;
    if(!world_pose(rig,head,next.world_from_head)||!inverse_rigid(head,from_head))return VR_CAMERA_BAD_RIG;
    for(unsigned i=0;i<2;++i){
        VrCameraEye* e=&next.eyes[i];double located[16];
        if(!pose_matrix(&s->eyes[i].pose,located))return VR_CAMERA_BAD_POSE;
        if(!world_pose(rig,located,e->world_from_eye)||!inverse_rigid(e->world_from_eye,e->view)||
           !multiply(from_head,located,e->head_from_eye))return VR_CAMERA_BAD_RIG;
        if(!projection(s,s->eyes[i].fov,e->projection))return VR_CAMERA_BAD_PROJECTION;
        if(native){e->projection[10]=native[10];e->projection[11]=native[11];}
        if(!multiply(e->projection,e->view,e->world_to_clip)||!planes(s,e))return VR_CAMERA_BAD_PROJECTION;
        memcpy(e->fov,s->eyes[i].fov,sizeof e->fov);
    }
    *out=next;return VR_CAMERA_OK;
}
VrCameraStatus vr_camera_build(const VrCameraRig* rig,const VrCameraSample* s,VrCameraFrame* out) {
    return build_frame(rig,s,NULL,out);
}
VrCameraStatus vr_camera_build_native_projection(const VrCameraRig* rig,const VrCameraSample* s,
                                                const double native[16],VrCameraFrame* out) {
    if(!native)return VR_CAMERA_BAD_ARGUMENT;
    return build_frame(rig,s,native,out);
}
int vr_camera_bounds_visible(const VrCameraFrame* frame,const double lo[3],const double hi[3]) {
    if(!frame||!lo||!hi||!frame->sequence||!finite_array(lo,3)||!finite_array(hi,3))return 1;
    for(unsigned k=0;k<3;++k)if(lo[k]>hi[k])return 1;
    for(unsigned eye=0;eye<2;++eye){
        int outside=0;
        for(unsigned i=0;i<6;++i){
            const double* p=frame->eyes[eye].planes[i];
            if(!frame->eyes[eye].plane_enabled[i])continue;
            if(!finite_array(p,4))return 1;
            double furthest=p[3];for(unsigned k=0;k<3;++k)furthest+=p[k]*(p[k]<0?lo[k]:hi[k]);
            if(!isfinite(furthest))return 1;
            if(furthest<0){outside=1;break;}
        }
        if(!outside)return 1;
    }
    return 0;
}
