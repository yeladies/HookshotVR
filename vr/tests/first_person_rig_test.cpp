// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_first_person_rig.h"
#include <cstdio>
#include <stdexcept>
static unsigned checks;
static void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
static bool near(double a,double b){return std::abs(a-b)<2e-6;}
// XrFrameSnapshot is an internal C++ type with padding between its bools and
// poses. Compare its defined fields rather than reading uninitialized padding.
static bool same_tracking(const mkwvk::XrFrameSnapshot& a,const mkwvk::XrFrameSnapshot& b){
    if(a.sequence!=b.sequence||a.origin_generation!=b.origin_generation||a.predicted_display_time!=b.predicted_display_time||
       a.observed_at_monotonic_ns!=b.observed_at_monotonic_ns||a.running!=b.running||a.focused!=b.focused||a.should_render!=b.should_render)return false;
    auto pose=[](const mkwvk::XrTrackedPose& x,const mkwvk::XrTrackedPose& y){
        return x.flags==y.flags&&!std::memcmp(x.orientation,y.orientation,sizeof x.orientation)&&!std::memcmp(x.position,y.position,sizeof x.position);};
    if(!pose(a.head,b.head))return false;
    for(unsigned k=0;k<2;++k)if(!pose(a.eyes[k].pose,b.eyes[k].pose)||std::memcmp(a.eyes[k].fov,b.eyes[k].fov,sizeof a.eyes[k].fov))return false;
    return true;
}
static bool same_projection(const mkwvk::FirstPersonProjectionPacket& a,const mkwvk::FirstPersonProjectionPacket& b){
    return a.game_present==b.game_present&&a.observed_ns==b.observed_ns&&a.synthetic==b.synthetic&&a.native_depth_preserved==b.native_depth_preserved&&same_tracking(a.tracking,b.tracking)&&
           !std::memcmp(&a.camera,&b.camera,sizeof a.camera);
}
static MkwVkFirstPersonInput input(){MkwVkFirstPersonInput i{};i.size=sizeof i;i.actor_id=123;i.game_present=1;
    i.root_position[0]=10;i.root_position[1]=20;i.root_position[2]=30;i.eye_height=1.5;i.forward_offset=.06;i.world_units_per_metre=1;return i;}
static mkwvk::XrFrameSnapshot sample(){mkwvk::XrFrameSnapshot s;s.sequence=1;s.origin_generation=1;s.predicted_display_time=200;s.observed_at_monotonic_ns=100;
    s.running=s.focused=s.should_render=true;s.head.orientation[3]=1;s.head.flags=3;s.head.position[1]=1.8f;
    for(unsigned k=0;k<2;++k){s.eyes[k].pose=s.head;s.eyes[k].pose.position[0]=k?.032f:-.032f;
        s.eyes[k].fov[0]=-.7f;s.eyes[k].fov[1]=.8f;s.eyes[k].fov[2]=.9f;s.eyes[k].fov[3]=-.6f;}return s;}
static MkwVkFirstPersonFrame output(){MkwVkFirstPersonFrame o{};o.size=sizeof o;return o;}
static void anchor(){
    mkwvk::FirstPersonRig rig;auto i=input();auto s=sample();auto o=output();
    check(rig.build(i,s,false,o),"initial build");check(o.flags==(MKWVK_FP_POSE_VALID|MKWVK_FP_RIG_RESET),"initial reset flag");
    check(near(o.eye_position[0],10)&&near(o.eye_position[1],21.5)&&near(o.eye_position[2],30.06),"root plus local bind eye, not tracking floor height");
    check(near(o.look_target[2]-o.eye_position[2],1),"Link +Z forward, XR -Z forward");
    const auto first=o;i.body_yaw=1.2;i.game_present++;i.root_position[0]+=3;
    s.head.position[0]+=.25f;s.head.position[1]+=.15f;
    check(rig.build(i,s,false,o),"root and physical displacement");
    check(near(o.eye_position[0],12.75)&&near(o.eye_position[1],21.65),"separate root and tracking translation");
    check(near(o.head_view[0],first.head_view[0])&&near(o.head_view[2],first.head_view[2]),"body rotation cannot auto-center view");
    const auto repeated=o;check(rig.build(i,s,false,o),"repeat same absolute sample");
    check(!std::memcmp(&repeated,&o,sizeof o),"no accumulation over repeated samples");
    // Absolute XrView poses already include IPD. Recover camera coordinates at
    // the root translation; separation must be 64 mm, scaled exactly once.
    check(near(o.eye_views[1][3]-o.eye_views[0][3],-.064),"located eye IPD once");
    i.world_units_per_metre=10;check(rig.build(i,s,false,o),"world scale");
    check(near(o.eye_views[1][3]-o.eye_views[0][3],-.64),"IPD world scaling once");
    check(near(o.eye_position[0],10.5)&&near(o.eye_position[1],23.0),"physical translation uses explicit world scale");
}
static void pitch_roll_and_alignment(){
    for(unsigned axis: {0u,2u}){auto i=input();auto s=sample();auto o=output();mkwvk::FirstPersonRig rig;
        s.head.orientation[3]=float(std::cos(.2));s.head.orientation[axis]=float(std::sin(.2));
        for(auto& eye:s.eyes)for(unsigned k=0;k<4;++k)eye.pose.orientation[k]=s.head.orientation[k];
        check(rig.build(i,s,false,o),"tilted initial head");
        check(axis==0?std::abs(o.head_view[6])>.3:std::abs(o.head_view[1])>.3,"initial pitch/roll retained");}
    auto i=input();auto s=sample();auto o=output();mkwvk::FirstPersonRig rig;
    s.head.orientation[3]=float(std::cos(.3));s.head.orientation[1]=float(std::sin(.3));
    for(auto& eye:s.eyes)for(unsigned k=0;k<4;++k)eye.pose.orientation[k]=s.head.orientation[k];
    check(rig.build(i,s,false,o),"initial tracking yaw");
    check(near(o.eye_position[0],10)&&near(o.eye_position[2],30.06),"eye forward offset follows anchor, not tracking origin yaw");
    check(near(o.look_target[0],10)&&near(o.look_target[2],31.06),"initial head yaw aligned to body facing");
    i.turn_radians=.3;i.game_present++;check(rig.build(i,s,false,o),"explicit yaw turn");const auto turn=o;
    check(rig.build(i,s,false,o)&&!std::memcmp(&turn,&o,sizeof o),"turn applied once per game present");
    i.game_present++;check(rig.build(i,s,false,o),"next manual turn");check(!near(o.head_view[2],turn.head_view[2]),"next present can turn again");
}
static void reset_and_failure(){
    auto i=input();auto s=sample();auto o=output();mkwvk::FirstPersonRig rig;check(rig.build(i,s,false,o),"base valid");
    i.eye_height=1.0;i.game_present++;check(rig.build(i,s,false,o)&&(o.flags&MKWVK_FP_RIG_RESET),"age height changes reset rig");
    i.actor_id++;check(rig.build(i,s,false,o)&&(o.flags&MKWVK_FP_RIG_RESET),"actor transition resets rig");
    s.origin_generation++;s.head.position[0]=100;check(rig.build(i,s,false,o)&&(o.flags&MKWVK_FP_RIG_RESET),"recenter resets reference");
    check(near(o.eye_position[0],10),"recenter does not teleport by 100 metres");
    const auto saved=o;const auto saved_rig=rig;
    s.eyes[1].pose.flags=1;i.turn_radians=.3;i.game_present++;
    check(!rig.build(i,s,false,o),"reject missing position");check(!std::memcmp(&saved,&o,sizeof o),"failed sample preserves output");
    s.eyes[1].pose.flags=3;auto expected=output();auto control=saved_rig;
    check(control.build(i,s,false,expected)&&rig.build(i,s,false,o),"recovery");check(!std::memcmp(&expected,&o,sizeof o),"failed sample did not accumulate turn/state");
    auto bad=i;bad.size--;check(!rig.build(bad,s,false,o),"ABI input size");
    bad=i;bad.flags=MKWVK_FP_SYNTHETIC_INPUT;check(!rig.build(bad,s,false,o),"synthetic identity cannot be relabelled real");
    check(rig.build(bad,s,true,o)&&(o.flags&MKWVK_FP_SYNTHETIC_OUTPUT)&&(o.flags&MKWVK_FP_RIG_RESET),"synthetic explicit provenance");
    s.focused=false;check(!rig.build(bad,s,true,o),"unfocused frame rejected");s.focused=true;
    bad.world_units_per_metre=NAN;check(!rig.build(bad,s,true,o),"non-finite scale rejected");
}
static void immutable_packet(){
    mkwvk::FirstPersonFrameOwner owner;auto i=input();auto s=sample();auto o=output();
    check(owner.build(i,s,false,o),"first immutable packet");const auto first=o;
    mkwvk::FirstPersonProjectionPacket projected{};
    check(owner.project(i.game_present,.125,1234,projected),"native projection packet");
    check(projected.camera.sequence==o.tracking_sequence&&projected.observed_ns==o.observed_ns&&!projected.synthetic,"projection provenance");
    check(projected.tracking.sequence==projected.camera.sequence&&projected.tracking.predicted_display_time==projected.camera.predicted_display_time,"layer poses retain exact render observation identity");
    check(near(projected.tracking.eyes[0].pose.position[0],-.032)&&near(projected.tracking.head.position[1],1.8),"tracking poses remain metres in LOCAL, separate from game-world root/scale");
    for(unsigned eye=0;eye<2;++eye)for(unsigned k=0;k<12;++k)check(near(projected.camera.eyes[eye].view[k],o.eye_views[eye][k]),"same frozen pose for host view and per-eye projection");
    check(projected.camera.near_z==.125&&projected.camera.far_z==1234,"authoritative native clipping retained");
    const auto projection_saved=projected;
    check(!owner.project(i.game_present+1,.125,1234,projected),"wrong present cannot borrow camera");
    check(!owner.project(i.game_present,0,1234,projected),"invalid native clip rejected");
    check(same_projection(projected,projection_saved),"failed projection is atomic");
    s.sequence++;s.observed_at_monotonic_ns++;s.predicted_display_time++;s.head.position[0]+=.5f;i.root_position[0]+=3;
    check(owner.build(i,s,false,o)&&!std::memcmp(&first,&o,sizeof o),"same game present cannot acquire a different pose, root or timestamp");
    check(owner.project(i.game_present,.125,1234,projected)&&same_projection(projected,projection_saved),"late tracking cannot replace frozen eye projections");
    i.game_present++;check(owner.build(i,s,false,o),"next game present camera packet");check(o.tracking_sequence==s.sequence&&!near(o.eye_position[0],first.eye_position[0]),"next present uses new tracking and root");
    s.origin_generation++;check(owner.build(i,s,false,o)&&(o.flags&MKWVK_FP_RIG_RESET),"recenter invalidates same-present cache");
    s.eyes[1].pose.flags=0;const auto saved=o;check(!owner.build(i,s,false,o)&&!std::memcmp(&saved,&o,sizeof o),"invalid tracking cannot reuse cache as a new valid observation");
}
static void native_projection(){
    mkwvk::FirstPersonFrameOwner owner;auto i=input();auto s=sample();auto o=output();
    i.world_units_per_metre=10;check(owner.build(i,s,false,o),"native depth owner");
    mkwvk::FirstPersonProjectionPacket original{},out{};
    check(owner.project(i.game_present,.13,9876,original),"reference only");
    double native[16];float source[16];
    for(unsigned k=0;k<16;++k)native[k]=source[k]=float(original.camera.eyes[0].projection[k]);
    native[0]=source[0]=.4f;native[5]=source[5]=.7f; // Discard game FOV, preserve depth.
    check(owner.project(i.game_present,0,0,out,native)&&out.native_depth_preserved,"native coefficients replace guessed clip arguments");
    check(same_tracking(out.tracking,s),"native projection keeps original layer poses in metres");
    const auto evidence=mkwvk::first_person_projection_evidence(out,o.head_view,source);
    check(evidence.available&&evidence.center_matches&&evidence.depth_exact&&evidence.fov_matches,"native projection matches frozen center and XR FOV");
    check(near(evidence.eye_separation_world,.64),"native eye separation uses units once");
    auto bad=out;bad.camera.eyes[1].projection[11]+=.001;
    check(!mkwvk::first_person_projection_evidence(bad,o.head_view,source).depth_exact,"detect altered depth");
    bad=out;bad.camera.eyes[0].projection[0]+=.001;
    check(!mkwvk::first_person_projection_evidence(bad,o.head_view,source).fov_matches,"detect game FOV leakage");
    auto changed=o;changed.head_view[3]+=1;
    check(!mkwvk::first_person_projection_evidence(out,changed.head_view,source).center_matches,"wrong camera cannot pass observation audit");
    bad=out;bad.tracking.sequence++;
    check(!mkwvk::first_person_projection_evidence(bad,o.head_view,source).available,"cannot relabel tracking identity");
    mkwvk::FirstPersonProjectionLookup lookup{};
    const auto saved=out;
    check(!owner.project(100,0,0,out,native,&lookup)&&lookup.status==1&&lookup.oldest_present==i.game_present&&lookup.newest_present==i.game_present,"missing present diagnosed without a nearest-pose fallback");
    native[8]=1;
    check(!owner.project(i.game_present,0,0,out,native,&lookup)&&same_projection(out,saved),"invalid native projection is atomic");
    check(lookup.status==2&&lookup.camera_status==VR_CAMERA_BAD_PROJECTION,"projection failure distinct from missing frame");
}
static void render_history(){
    mkwvk::FirstPersonFrameOwner owner;auto i=input();auto s=sample();auto o=output();
    check(owner.build(i,s,false,o),"history first frame");mkwvk::FirstPersonProjectionPacket first{},out{};
    check(owner.project(1,.125,1234,first),"history initial projection");
    for(unsigned k=0;k<63;++k){i.game_present++;s.sequence++;s.observed_at_monotonic_ns++;s.head.position[0]+=.01f;check(owner.build(i,s,false,o),"advance retained frames");}
    check(owner.project(1,.125,1234,out)&&same_projection(out,first),"GPU can consume old frame with its original pose and timestamp");
    i.game_present++;s.sequence++;check(owner.build(i,s,false,o),"history overflow");
    check(!owner.project(1,.125,1234,out)&&owner.project(i.game_present,.125,1234,out),"oldest expires and newest remains");
    const auto saved_input=i;i.game_present--;check(!owner.build(i,s,false,o),"late host query cannot replace newer game present");i=saved_input;
    s.origin_generation++;check(owner.build(i,s,false,o),"history recenter");
    check(!owner.project(i.game_present-1,.125,1234,out)&&owner.project(i.game_present,.125,1234,out)&&out.camera.origin_generation==s.origin_generation,"recenter invalidates old-origin history");
}
int main(){try{native_projection();render_history();immutable_packet();anchor();pitch_roll_and_alignment();reset_and_failure();std::printf("First-person rig: %u checks passed\n",checks);return 0;}
catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
