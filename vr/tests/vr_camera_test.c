/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vr_camera.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)){fprintf(stderr,"camera check failed at %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static int close_to(double a,double b){return fabs(a-b)<1e-8;}
static VrCameraRig rig(void){VrCameraRig r;memset(&r,0,sizeof r);r.world_units_per_metre=1;r.origin_generation=7;return r;}
static VrCameraSample sample(void){
    VrCameraSample s;memset(&s,0,sizeof s);s.sequence=17;s.origin_generation=7;s.predicted_display_time=123456;
    s.head.orientation[3]=1;s.head.position[1]=1.6;s.head.flags=3;
    for(unsigned i=0;i<2;++i){s.eyes[i].pose=s.head;s.eyes[i].pose.position[0]=i?.032:-.032;
        s.eyes[i].fov[0]=-.8;s.eyes[i].fov[1]=.7;s.eyes[i].fov[2]=.6;s.eyes[i].fov[3]=-.9;}
    s.near_z=.1;s.far_z=25000;return s;
}
static void transform(const double m[16],const double p[4],double out[4]){
    for(unsigned r=0;r<4;++r){out[r]=0;for(unsigned k=0;k<4;++k)out[r]+=m[4*r+k]*p[k];}
}
static void eye_origin(const VrCameraEye* e){
    const double p[4]={e->world_from_eye[3],e->world_from_eye[7],e->world_from_eye[11],1};double out[4];
    transform(e->view,p,out);CHECK(close_to(out[0],0)&&close_to(out[1],0)&&close_to(out[2],0)&&close_to(out[3],1));
}
static void coordinates(void){
    VrCameraRig r=rig();VrCameraSample s=sample();VrCameraFrame f;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
    CHECK(close_to(f.eyes[1].world_from_eye[3]-f.eyes[0].world_from_eye[3],.064));
    CHECK(close_to(f.eyes[0].head_from_eye[3],-.032)&&close_to(f.eyes[1].head_from_eye[3],.032));
    CHECK(close_to(f.world_from_head[7],1.6));eye_origin(&f.eyes[0]);eye_origin(&f.eyes[1]);
    r.position_world[0]=1000000;r.position_world[1]=25;r.position_world[2]=-600;
    r.tracking_reference_m[1]=1.6;r.world_units_per_metre=10;r.yaw_radians=acos(-1.0)/2;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
    CHECK(close_to(f.eyes[0].world_from_eye[11],-599.68)&&close_to(f.eyes[1].world_from_eye[11],-600.32));
    CHECK(close_to(f.world_from_head[7],25));eye_origin(&f.eyes[0]);eye_origin(&f.eyes[1]);
    /* Runtime eye poses are already rotated. Adding IPD a second time fails this. */
    s.head.orientation[1]=sqrt(.5);s.head.orientation[3]=sqrt(.5);
    for(unsigned i=0;i<2;++i){s.eyes[i].pose=s.head;s.eyes[i].pose.position[2]=i?-.032:.032;}
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
    CHECK(close_to(f.eyes[0].world_from_eye[3],1000000.32)&&close_to(f.eyes[1].world_from_eye[3],999999.68));
    CHECK(close_to(f.eyes[0].head_from_eye[3],-.032)&&close_to(f.eyes[1].head_from_eye[3],.032));
    CHECK(close_to(f.eyes[0].head_from_eye[11],0));
    /* Absolute initial pitch and roll are retained, not canceled at anchoring. */
    r=rig();s=sample();s.head.orientation[0]=sqrt(.5);s.head.orientation[3]=sqrt(.5);
    s.eyes[0].pose=s.head;s.eyes[1].pose=s.head;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);CHECK(close_to(f.world_from_head[6],-1)&&close_to(f.world_from_head[9],1));
    s.head.orientation[0]=0;s.head.orientation[2]=sqrt(.5);
    s.eyes[0].pose=s.head;s.eyes[1].pose=s.head;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);CHECK(close_to(f.world_from_head[1],-1)&&close_to(f.world_from_head[4],1));
    VrCameraFrame first=f;
    for(unsigned i=0;i<500;++i){CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);CHECK(!memcmp(&f,&first,sizeof f));}
    /* Room translation beyond the old one-metre cutoff remains a valid pose. */
    s.head.position[0]=3;s.eyes[0].pose=s.head;s.eyes[1].pose=s.head;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK&&close_to(f.world_from_head[3],3));
}
static void projection_edges(void){
    VrCameraRig r=rig();VrCameraSample s=sample();VrCameraFrame f;
    for(unsigned clip=0;clip<2;++clip)for(unsigned reversed=0;reversed<2;++reversed)for(unsigned flip=0;flip<2;++flip){
        s.clip=(VrCameraClip)clip;s.depth=(VrCameraDepth)reversed;s.flip_y=flip;
        CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
        CHECK(f.near_z==s.near_z&&f.far_z==s.far_z);
        for(unsigned eye=0;eye<2;++eye){const double* p=f.eyes[eye].projection;double out[4];
            for(unsigned corner=0;corner<4;++corner){
                double v[4]={tan(s.eyes[eye].fov[corner&1])*s.near_z,tan(s.eyes[eye].fov[2+(corner>>1)])*s.near_z,-s.near_z,1};
                transform(p,v,out);CHECK(close_to(out[0]/out[3],corner&1?1:-1));
                CHECK(close_to(out[1]/out[3],(corner>>1?-1:1)*(flip?-1:1)));
            }
            double near_point[4]={0,0,-s.near_z,1},far_point[4]={0,0,-s.far_z,1};
            transform(p,near_point,out);CHECK(close_to(out[2]/out[3],reversed?1:(clip?0:-1)));
            transform(p,far_point,out);CHECK(close_to(out[2]/out[3],reversed?(clip?0:-1):1));
        }
    }
    /* Different asymmetric optics, including canted eye orientation, survive unchanged. */
    s=sample();s.eyes[1].fov[0]=-.5;s.eyes[1].fov[1]=1.0;
    s.eyes[1].pose.orientation[1]=sin(.1);s.eyes[1].pose.orientation[3]=cos(.1);
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
    CHECK(f.eyes[0].projection[2]!=f.eyes[1].projection[2]&&f.eyes[1].fov[0]==-.5);
}
static int point_visible(const VrCameraFrame* f,double x,double y,double z){double lo[3]={x,y,z},hi[3]={x,y,z};return vr_camera_bounds_visible(f,lo,hi);}
static void visibility(void){
    VrCameraRig r=rig();VrCameraSample s=sample();VrCameraFrame f;
    s.far_z=100;
    for(unsigned i=0;i<2;++i){s.eyes[i].pose.position[0]=i?.2:-.2;
        s.eyes[i].fov[0]=s.eyes[i].fov[3]=-atan(.25);s.eyes[i].fov[1]=s.eyes[i].fov[2]=atan(.25);}
    for(unsigned clip=0;clip<2;++clip)for(unsigned reversed=0;reversed<2;++reversed){
        s.clip=(VrCameraClip)clip;s.depth=(VrCameraDepth)reversed;CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);
        CHECK(point_visible(&f,.4,1.6,-1));CHECK(point_visible(&f,-.4,1.6,-1)); /* one eye only */
        CHECK(!point_visible(&f,.6,1.6,-1));CHECK(!point_visible(&f,0,1.6,1));
        CHECK(!point_visible(&f,0,1.6,-101));CHECK(!point_visible(&f,.2,1.6,-.05));
        s.far_z=INFINITY;CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);CHECK(point_visible(&f,0,1.6,-1e9));
        CHECK(!f.eyes[0].plane_enabled[5]);s.far_z=100;
    }
    double lo[3]={NAN,0,0},hi[3]={1,1,1};CHECK(vr_camera_bounds_visible(&f,lo,hi));
}
static void native_depth(void){
    VrCameraRig r=rig();VrCameraSample s=sample();VrCameraFrame source,out,saved;
    for(unsigned clip=0;clip<2;++clip)for(unsigned reverse=0;reverse<2;++reverse)for(unsigned infinite=0;infinite<2;++infinite){
        s=sample();s.clip=(VrCameraClip)clip;s.depth=(VrCameraDepth)reverse;s.far_z=infinite?INFINITY:23456.7;
        CHECK(vr_camera_build(&r,&s,&source)==VR_CAMERA_OK);
        double native[16];for(unsigned i=0;i<16;++i)native[i]=(double)(float)source.eyes[0].projection[i];
        /* Native XY/FOV and jitter must not leak into either XR eye. Depth is
           copied bit-exact as a double representation of the source float. */
        native[0]=.75;native[5]=-.9;native[2]=.001;native[6]=-.002;
        s.near_z=NAN;s.far_z=NAN;s.flip_y=0;
        CHECK(vr_camera_build_native_projection(&r,&s,native,&out)==VR_CAMERA_OK);
        CHECK(out.eyes[0].projection[0]!=native[0]&&out.eyes[0].projection[5]>0);
        for(unsigned eye=0;eye<2;++eye){
            CHECK(!memcmp(out.eyes[eye].projection+10,native+10,2*sizeof(double)));
            CHECK(out.eyes[eye].projection[14]==-1&&out.eyes[eye].projection[15]==0);
            double at_near[4]={0,0,-out.near_z,1},result[4];transform(out.eyes[eye].projection,at_near,result);
            const double expected=reverse?1:(clip?0:-1);CHECK(close_to(result[2]/result[3],expected));
            CHECK(infinite?!out.eyes[eye].plane_enabled[5]:out.eyes[eye].plane_enabled[5]);
        }
        saved=out;
        for(unsigned bad=0;bad<7;++bad){
            double malformed[16];memcpy(malformed,native,sizeof malformed);
            if(bad==0)malformed[8]=.1; /* oblique depth is not supported */
            if(bad==1)malformed[14]=0;
            if(bad==2)malformed[11]=0;
            if(bad==3)malformed[10]=NAN;
            if(bad==4)malformed[0]=0;
            if(bad==5)malformed[11]=-malformed[11];
            if(bad==6)malformed[15]=1;
            CHECK(vr_camera_build_native_projection(&r,&s,malformed,&out)==VR_CAMERA_BAD_PROJECTION);
            CHECK(!memcmp(&out,&saved,sizeof out));
        }
        CHECK(vr_camera_build_native_projection(&r,&s,NULL,&out)==VR_CAMERA_BAD_ARGUMENT);
        CHECK(!memcmp(&out,&saved,sizeof out));
    }
}
static void failures_are_atomic(void){
    VrCameraRig r=rig();VrCameraSample s=sample();VrCameraFrame f,saved;
    CHECK(vr_camera_build(&r,&s,&f)==VR_CAMERA_OK);saved=f;
#define REJECT(status) do {CHECK(vr_camera_build(&r,&s,&f)==(status));CHECK(!memcmp(&f,&saved,sizeof f));} while(0)
    r.world_units_per_metre=0;REJECT(VR_CAMERA_BAD_RIG);r=rig();
    r.origin_generation=8;REJECT(VR_CAMERA_BAD_IDENTITY);r=rig();
    s.sequence=0;REJECT(VR_CAMERA_BAD_IDENTITY);s=sample();
    s.predicted_display_time=0;REJECT(VR_CAMERA_BAD_IDENTITY);s=sample();
    s.eyes[1].pose.flags=VR_CAMERA_ORIENTATION_VALID;REJECT(VR_CAMERA_BAD_POSE);s=sample();
    s.head.orientation[3]=0;REJECT(VR_CAMERA_BAD_POSE);s=sample();
    s.head.position[0]=NAN;REJECT(VR_CAMERA_BAD_POSE);s=sample();
    s.eyes[0].fov[1]=s.eyes[0].fov[0];REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    s.near_z=0;REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    s.far_z=s.near_z;REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    s.far_z=-INFINITY;REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    s.clip=(VrCameraClip)2;REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    s.flip_y=2;REJECT(VR_CAMERA_BAD_PROJECTION);s=sample();
    CHECK(vr_camera_build(NULL,&s,&f)==VR_CAMERA_BAD_ARGUMENT);CHECK(!memcmp(&f,&saved,sizeof f));
#undef REJECT
}
int main(void){coordinates();projection_edges();visibility();native_depth();failures_are_atomic();printf("Decoupled C camera: %u checks passed\n",checks);return 0;}
