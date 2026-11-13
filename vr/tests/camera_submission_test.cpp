// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_submission.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <cstdlib>
#include <string>
using namespace mkwvk;
namespace mkwvk {void logf(const char*,...){} void xr_immersive_note_camera(uint64_t,const float*,size_t){} void xr_immersive_note_scene_candidate(uint64_t){}}
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
CameraCapturePair pair(uint64_t seq,uint64_t id=7){CameraCapturePair p{};p.revision=seq;p.primary.sequence=seq;p.primary.camera_id=id;
    p.primary.view[0]=p.primary.view[5]=p.primary.view[10]=1;p.uniforms=p.primary;p.uniforms.camera_id=9;
    p.uniforms.projection[0]=1;p.uniforms.projection[5]=1.7f;p.uniforms.projection[10]=-1.01f;p.uniforms.projection[11]=-.2f;p.uniforms.projection[14]=-1;return p;}
int main(){try {
    constexpr int64_t t=1000000000;
    SceneCameraHistory history;auto a=pair(1);history.observe(a,t);
    const auto first=history.snapshot(t+1);check(first.count==1,"candidate copied");
    auto b=pair(2);b.primary.view[3]=-3;history.observe(b,t+2);
    check(first.count==1&&first.cameras[0].pair.primary.view[3]==0,"new camera cannot mutate earlier submit");
    check(history.snapshot(t-1).count==0,"future observations excluded");
    check(history.snapshot(t+300000000).count==0,"old observations expire at capture");
    float block[0x230/4]{};std::copy_n(a.primary.view,12,block);std::copy_n(a.uniforms.projection,16,block+0x70/4);
    std::copy_n(a.uniforms.projection,16,block+0x30/4);block[0x120/4]=1280;block[0x124/4]=720;
    SceneCameraEvidence draw{1,2,3,4,5,1280,720};
    check(match_submitted_camera(&first,t+1000,draw,block,0x230/4).matched,"exact submitted camera candidate");
    check(match_submitted_uniform(&first,t+1000,1280,720,block,0x230/4).matched,"compute uniform can match without claiming attachments");
    check(!match_submitted_uniform(&first,t+1000,640,720,block,0x230/4).matched,"compute dimensions must agree");
    auto missing_draw=draw;missing_draw.depth=0;
    check(!match_submitted_camera(&first,t+1000,missing_draw,block,0x230/4).matched,"uniform match cannot fabricate scene evidence");
    check(!match_submitted_uniform(&first,t+600000000,1280,720,block,0x230/4).matched,"compute context expires too");
    check(!match_submitted_camera(&first,t+600000000,draw,block,0x230/4).matched,"delayed context fails closed");
    check(!match_submitted_camera(&first,t-1,draw,block,0x230/4).matched,"clock reversal fails closed");
    history.observe(pair(3),t+3);auto equivalent=history.snapshot(t+4);
    check(match_submitted_camera(&equivalent,t+5,draw,block,0x230/4).equivalent==2,"stationary duplicate observations remain explicit");
    history.observe(pair(4,88),t+5);auto ambiguous=history.snapshot(t+6);
    auto m=match_submitted_camera(&ambiguous,t+7,draw,block,0x230/4);check(m.ambiguous&&!m.matched,"different camera identities cannot establish ownership");
    check(!match_submitted_uniform(&ambiguous,t+7,1280,720,block,0x230/4).matched,"compute rejects ambiguous identities");
    history.clear();for(uint64_t i=0;i<100;++i)history.observe(pair(i+1),t+i);
    check(history.snapshot(t+101).count==16,"history bounded");
    SceneSubmissionStore store;auto pinned=std::make_shared<const SceneSubmissionSnapshot>(first);
    store.bind(1,pinned);for(uint64_t i=2;i<=258;++i)store.bind(i,pinned);
    check(!store.get(1)&&store.get(258)==pinned,"bounded store evicts without a latest fallback");
    check(pinned->count==1,"eviction leaves held snapshots alive");
#ifdef _WIN32
    _putenv_s("HOOKSHOT_VR_SCENE_CAPTURE","1");
#else
    setenv("HOOKSHOT_VR_SCENE_CAPTURE","1",1);
#endif
    a.primary.size=sizeof(MkwVkCameraObservation);a.primary.flags=MKWVK_CAMERA_PRIMARY;a.primary.game_present=1;a.primary.sequence=1;
    a.uniforms.size=sizeof(MkwVkCameraObservation);a.uniforms.flags=MKWVK_CAMERA_UNIFORM;a.uniforms.game_present=1;a.uniforms.sequence=2;
    xr_camera_observation(&a.primary);xr_camera_observation(&a.uniforms);
    auto live=xr_scene_submission_capture();check(live&&live->count==1,"actual callback feeds snapshot");
    xr_scene_submission_bind(10,live);check(!xr_scene_submission_current(),"no context outside replay scope");
    {SceneSubmissionScope scope(10);check(xr_scene_submission_current()==live,"source submission scope");
     {SceneSubmissionScope missing(11);check(!xr_scene_submission_current(),"unknown submit does not borrow another context");}
     check(xr_scene_submission_current()==live,"nested scope restored");
     auto resumed=std::make_shared<const SceneSubmissionSnapshot>(history.snapshot(t+101));
     xr_scene_submission_bind(10,resumed);
     check(xr_scene_submission_current()==live,"refresh never mutates an already active scope");
     bool refreshed=false;
     std::thread resume([&]{SceneSubmissionScope scope(10);refreshed=xr_scene_submission_current()==resumed;});resume.join();
     check(refreshed,"resumed work selects refreshed context on its own thread");
     std::thread other([]{check(!xr_scene_submission_current(),"parallel worker has no accidental context");});other.join();}
    check(!xr_scene_submission_current(),"scope ends deterministically");
    std::cout<<"Bounded submit-time camera snapshots and scope isolation checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
