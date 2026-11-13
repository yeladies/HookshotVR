// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_preview_feed.h"
#include "../../host/vr_camera_read.h"
#include <iostream>
#include <stdexcept>
using namespace mkwvk;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){try{
    constexpr int64_t t=1000000000;PreviewFeed feed;auto left=std::make_shared<std::vector<uint8_t>>(32,17),right=std::make_shared<std::vector<uint8_t>>(32,85);
    ImmersiveTag tag;tag.valid=true;tag.temporal_stereo=true;tag.generation=1;tag.observed_ns=t;tag.eye=0;
    check(feed.publish(left,4,2,10,t,tag),"left frame");check(!feed.latest(t+1).pixels,"one eye alone never produces stereo");
    tag.eye=1;check(feed.publish(right,4,2,12,t+10000000,tag),"right frame");auto pair=feed.latest(t+10000001);
    check(pair.pixels==left&&pair.right_pixels==right&&pair.immersive.eye==0&&pair.right_immersive.eye==1,"different original images stay routed to their eye");
    check(!feed.publish(right,4,2,12,t+11000000,tag),"duplicate eye present rejected");
    ImmersiveTag next=tag;next.eye=0;next.observed_ns=t+20000000;
    check(feed.publish(left,4,2,20,t+20000000,next),"new eye with temporarily excessive gap");
    auto held=feed.latest(t+21000000);check(held.serial==pair.serial&&held.right_serial==pair.right_serial,"fresh committed pair survives one unmatched update");
    check(!feed.publish(left,4,2,21,t+22000000,{}),"unassociated image cannot replace a tracked pair");
    check(feed.latest(t+23000000).serial==pair.serial,"unassociated frame does not blank fresh provenance");
    check(feed.stats().held>0&&feed.stats().gap_requests>0&&feed.stats().unassociated==1,"pair diagnostics distinguish holds, gaps and unassociated frames");
    check(!feed.publish(right,4,2,10,t+12000000,tag),"same-source-frame fake stereo rejected");
    check(!feed.latest(t+210000000).pixels,"old opposite eye rejected");
    tag.generation=2;check(feed.publish(right,4,2,14,t+20000000,tag),"new generation");check(!feed.latest(t+20000001).pixels,"origins cannot mix");
    tag.eye=0;tag.observed_ns=t+30000000;check(feed.publish(left,4,2,24,t+30000000,tag),"distant present");check(!feed.latest(t+30000001).pixels,"distant game presents cannot pair");
    ImmersiveTag menu;menu.menu=true;menu.generation=2;check(feed.publish(left,4,2,25,t+40000000,menu),"menu frame");check(feed.latest(t+40000001).immersive.menu,"menu can display without stereo provenance");
    tag.eye=1;tag.observed_ns=t+50000000;feed.publish(right,4,2,26,t+50000000,tag);check(!feed.latest(t+50000001).pixels,"closing menu requires two new eye images");
    const float view0[12]={1,0,0,0,0,1,0,0,0,0,1,0};float view[12];std::memcpy(view,view0,sizeof view);ImmersiveCamera camera;ImmersivePose anchor;anchor.generation=1;anchor.observed_ns=t;
    ImmersivePose eye=anchor;eye.temporal_stereo=true;eye.p[0]=-.032f;
    check(camera.apply(7,view,eye,t,&anchor)&&std::abs(view[3]-.032f)<.00001,"left camera uses tracked eye position relative to head centre");
    eye.eye=1;eye.p[0]=.032f;std::memcpy(view,view0,sizeof view);check(camera.apply(7,view,eye,t,&anchor)&&std::abs(view[3]+.032f)<.00001,"right eye is not a duplicate or a second added IPD");
    float projection[16]={1,0,0,0,0,1,0,0,0,0,-1,-.2f,0,0,-1,0};auto found=camera.match(view,projection,t);check(found.valid&&found.temporal_stereo&&found.eye==1,"source association retains eye identity");
    const float lf[4]={-.7f,.8f,.9f,-.6f},rf[4]={-.8f,.7f,.8f,-.9f};float fov=0,aspect=0;check(immersive_lens_from_fovs(lf,rf,fov,aspect)&&std::abs(fov-1.8f)<.0001,"lens covers both runtime fields of view");
    float wide=0,wide_aspect=0;check(immersive_lens_from_fovs(lf,rf,wide,wide_aspect,1.25f)&&wide>fov&&std::abs(wide_aspect-aspect)<.00001,"optical margin widens actual source lens without stretching aspect");
    const auto saved_wide=wide;check(!immersive_lens_from_fovs(lf,rf,wide,wide_aspect,1.5f)&&wide==saved_wide,"unsupported margin rejected atomically");
    float bad[4]={0,.7f,.8f,-.7f};const float saved=fov;check(!immersive_lens_from_fovs(bad,rf,fov,aspect)&&fov==saved,"bad lens leaves outputs untouched");
    float engine_fov=0,engine_aspect=0;check(host_vr_lens_fields(1.745329252f,1.19836f,&engine_fov,&engine_aspect)&&std::abs(engine_fov-1.191753593f)<.00001,"100 degree lens is converted to engine half-angle tangent");
    const float old_engine=engine_fov;check(!host_vr_lens_fields(3.2f,1.f,&engine_fov,&engine_aspect)&&engine_fov==old_engine,"unsupported lens leaves engine fields unchanged");
    std::cout<<"Temporal stereo preserves two images, eye poses, age, origin, present bounds and runtime lens coverage\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
