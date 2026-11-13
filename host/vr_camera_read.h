/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HOOKSHOT_VR_CAMERA_READ_H
#define HOOKSHOT_VR_CAMERA_READ_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
typedef int (*HostVrRead)(uint64_t,void*,size_t,void*);
typedef struct HostVrPrimary {uint64_t camera;float rows[12];double eye[3];} HostVrPrimary;
static inline int host_vr_at(HostVrRead read,void* context,uint64_t base,uint64_t offset,void* out,size_t size) {
    return base&&base<=UINT64_MAX-offset&&read(base+offset,out,size,context);
}
static inline int host_vr_primary_once(HostVrRead read,void* context,HostVrPrimary* out) {
    uint64_t singleton=0,manager=0,cameras=0,camera=0,view=0,vtable=0;uint32_t count=0;
    if(!read(0x84ca6bc0ULL,&singleton,sizeof singleton,context)||!host_vr_at(read,context,singleton,0,&manager,sizeof manager)||
       !host_vr_at(read,context,manager,32,&count,sizeof count)||!count||count>64||
       !host_vr_at(read,context,manager,40,&cameras,sizeof cameras)||!host_vr_at(read,context,cameras,0,&camera,sizeof camera)||
       !host_vr_at(read,context,camera,120,&view,sizeof view)||!host_vr_at(read,context,view,0,&vtable,sizeof vtable)||vtable!=0x84c18a88ULL)return 0;
    HostVrPrimary sample;sample.camera=view;
    if(!host_vr_at(read,context,view,8,sample.rows,sizeof sample.rows)||!host_vr_at(read,context,view,96,sample.eye,sizeof sample.eye))return 0;
    for(int i=0;i<12;++i)if(!isfinite(sample.rows[i]))return 0;
    for(int i=0;i<3;++i)if(!isfinite(sample.eye[i]))return 0;
    *out=sample;return 1;
}
static inline int host_vr_primary(HostVrRead read,void* context,HostVrPrimary* out) {
    HostVrPrimary first,second;
    if(!host_vr_primary_once(read,context,&first)||!host_vr_primary_once(read,context,&second)||first.camera!=second.camera||
       memcmp(first.rows,second.rows,sizeof first.rows)||memcmp(first.eye,second.eye,sizeof first.eye))return 0;
    *out=second;return 1; /* Still observational: two matching reads are not a transaction across guest writers. */
}
typedef struct HostVrCameraCache {uint64_t primary,uniform;float rows[12];} HostVrCameraCache;
static inline void host_vr_cache_primary(HostVrCameraCache* cache,uint64_t camera,const float* rows,int debug) {
    if(!camera||!rows||debug){memset(cache,0,sizeof *cache);return;}
    if(cache->primary!=camera)cache->uniform=0;
    cache->primary=camera;memcpy(cache->rows,rows,sizeof cache->rows);
}
static inline void host_vr_cache_uniform(HostVrCameraCache* cache,uint64_t object,const float* rows) {
    if(!cache->primary||!object||!rows)return;
    for(int i=0;i<12;++i)if(!isfinite(rows[i])||!isfinite(cache->rows[i])||fabs((double)rows[i]-cache->rows[i])>(i%4==3?0.01:0.0001))return;
    cache->uniform=object;
}
#endif

/* Sead's perspective +0xac stores tan(vertical_fov/2), not the angle.
   Keep conversion checked and atomic before writing the register/frustum fields. */
static inline int host_vr_lens_fields(float vertical_radians,float ratio,float* tangent,float* aspect) {
    if(!tangent||!aspect||!isfinite(vertical_radians)||!isfinite(ratio)||vertical_radians<=0.2f||vertical_radians>=2.8f||ratio<0.5f||ratio>2.5f)return 0;
    const double value=tan((double)vertical_radians*0.5);
    if(!isfinite(value)||value<=0.2||value>=2.8)return 0;
    *tangent=(float)value;*aspect=ratio;return 1;
}
