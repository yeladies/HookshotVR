// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "vk_xr.h"
namespace mkwvk {bool env_on(const char*);}
#define HOOKSHOT_PAIR_FIXTURE_ONLY
#define HOOKSHOT_PAIR_ENV_NAME pair_fixture_env_on
#include "gpu_depth_pair_probe_test.cpp"
#include "xr_gpu_eye_stream.h"
namespace mkwvk {
bool fake_xr_active=false,stream_probe=true;XrFrameSnapshot stream_snapshot;
bool env_on(const char* name){return std::strcmp(name,"RX_VK_XR_GPU_EYE_STREAM")==0||(stream_probe&&(std::strcmp(name,"RX_VK_XR_GPU_EYE_STREAM_PROBE")==0||std::strcmp(name,"HOOKSHOT_VR_IMMERSIVE_TEST")==0));}
bool xr_vulkan_active(){return fake_xr_active;}
bool xr_vulkan_latest_frame(XrFrameSnapshot* out){*out=stream_snapshot;return stream_snapshot.sequence!=0;}
}
#include "gpu_eye_stream_function.inc"
using namespace mkwvk;
void setting(const char* name,const char* value){
#ifdef _WIN32
    _putenv_s(name,value);
#else
    setenv(name,value,1);
#endif
}
void reset_stream(){auto& p=eye_stream();p.~EyeStream();new(&p)EyeStream;reset_pair();fake_xr_active=false;stream_probe=true;stream_snapshot={};setting("HOOKSHOT_VR_GPU_EYE_WIDTH","8");setting("HOOKSHOT_VR_GPU_EYE_HEIGHT","8");setting("HOOKSHOT_VR_GPU_EYE_STREAM_IMAGE_BASE","");fake.pose.valid=true;fake.pose.generation=1;fake.pose.observed_ns=gpu_scene_now();fake.synthetic=true;}
void record_stream(uint64_t frame){fake.pose.observed_ns=gpu_scene_now();capture(frame);complete(frame);xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),frame*10+5,0xfd029a317a9a4b94ull);}
void finish_stream(uint64_t frame){fake.completed=frame*10+5;fake.frame=frame+2;xr_gpu_eye_stream_tick(frame+2);}
auto output(){return xr_gpu_eye_stream_acquire(dev.device,1,eye_stream_now(),true);}
void stream_clean(){check(eye_stream().allocated==0&&pair_fake.pipelines.empty()&&pair_fake.layouts.empty()&&pair_fake.shaders.empty(),"continuous resources released");}
int main(){try{
    reset_stream();check(xr_gpu_eye_stream_stop(),"empty stop");
    for(int failure=1;failure<=7;++failure){reset_stream();capture(1);complete(1);fake.fail=fake.calls+failure;xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),15,0xfd029a317a9a4b94ull);check(eye_stream().failures==1&&pair_fake.dispatches==0,"pipeline/slot setup failures record no partial eye pair");check(xr_gpu_eye_stream_stop(),"failed setup can stop without GPU work");stream_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"original source remains owned and releasable");}
    {reset_stream();record_stream(1);check(!output()&&eye_stream().slots[0].source&&eye_stream().recorded==1,"pending pair retains its captured source and stays unpublished");finish_stream(1);auto a=output();check(a&&a->state==1&&a->source_state==1&&a->width==8&&a->height==8&&!eye_stream().slots[0].source,"ready pair releases input and publishes exact output dimensions");check(!xr_gpu_eye_stream_acquire(dev.device,1,eye_stream_now()),"synthetic frames cannot enter default headset acquisition");
        const auto saved=*a;record_stream(3);finish_stream(3);auto b=output();record_stream(5);finish_stream(5);auto c=output();check(a->state==1&&b->state==2&&c->state==3,"leased metadata stays immutable across continuous production");record_stream(7);check(eye_stream().recorded==3&&eye_stream().leased_skips==1,"three borrowed frames apply backpressure");std::weak_ptr<const XrGpuEyeFrame> weak=a;a.reset();record_stream(9);check(weak.expired()&&eye_stream().recorded==3,"copied frame value also pins the slot");b.reset();record_stream(11);finish_stream(11);check(eye_stream().recorded==4&&saved.state==1&&saved.buffer!=output()->buffer,"only an unborrowed slot is reused");check(!xr_gpu_eye_stream_acquire(dev.device,2,eye_stream_now(),true)&&!xr_gpu_eye_stream_acquire(dev.device,1,eye_stream_now()+300000000,true),"wrong origin and stale source rejected");check(!xr_gpu_eye_stream_stop(),"borrowed output survives stop");}
    check(xr_gpu_eye_stream_stop(),"last copied/borrowed frame release permits cleanup");stream_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"all continuous and source storage retired");
    for(int failure=1;failure<=4;++failure){reset_stream();record_stream(1);finish_stream(1);const auto base=std::filesystem::temp_directory_path()/("vr-own-stream-"+std::to_string(eye_stream_now()));setting("HOOKSHOT_VR_GPU_EYE_STREAM_IMAGE_BASE",base.string().c_str());fake.fail=fake.calls+4+failure;xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),20,0xfd029a317a9a4b94ull);check(eye_stream().stopped&&eye_stream().failures==1&&eye_stream().recorded==2,"probe allocation/map failure occurs after an actual output job was recorded");check(!xr_gpu_eye_stream_stop(),"failed optional readback cannot free the newly pending GPU job");fake.completed=20;check(xr_gpu_eye_stream_stop(),"known completion releases failed optional readback case");stream_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"optional readback failure leaves source storage releasable");}
    reset_stream();record_stream(1);check(!xr_gpu_eye_stream_stop()&&eye_stream().slots[0].source,"uncertain GPU use retains original source and output");fake.completed=15;check(xr_gpu_eye_stream_stop(),"known completion permits shutdown");stream_clean();
    reset_stream();fake_xr_active=true;record_stream(1);check(!eye_stream().recorded&&eye_stream().pose_rejections==1,"synthetic production refuses an active XR session");check(xr_gpu_eye_stream_stop(),"unsubmitted synthetic rejection cleans up");stream_clean();
    reset_stream();record_stream(1);finish_stream(1);dev.device=handle<VkDevice>(2);check(!xr_gpu_eye_stream_acquire(handle<VkDevice>(1),1,eye_stream_now(),true),"acquisition notices abandoned device");dev.device=handle<VkDevice>(1);check(!output()&&!xr_gpu_eye_stream_stop()&&!pair_fake.pipelines.empty(),"recycled device handle never revives abandoned storage");
    reset_stream();stream_probe=false;fake.synthetic=false;stream_snapshot.sequence=19;stream_snapshot.origin_generation=1;stream_snapshot.running=stream_snapshot.focused=stream_snapshot.should_render=true;stream_snapshot.head.flags=3;
    for(unsigned eye=0;eye<2;++eye){auto& e=stream_snapshot.eyes[eye];e.pose.flags=3;e.pose.orientation[3]=1;e.pose.position[0]=eye?.032f:-.032f;const float fov[4]={-.7f,.7f,.7f,-.7f};std::copy(fov,fov+4,e.fov);}
    fake.pose.observed_ns=gpu_scene_now();capture(1);complete(1);stream_snapshot.observed_at_monotonic_ns=gpu_scene_now();xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),15,0xfd029a317a9a4b94ull);finish_stream(1);auto real=xr_gpu_eye_stream_acquire(dev.device,1,eye_stream_now());check(real&&!real->synthetic&&real->target.serial==19&&real->target.eyes[0].fov[0]==-.7f,"real-mode mailbox preserves common current pose sample and per-eye runtime FOV");real.reset();check(xr_gpu_eye_stream_stop(),"real-mode output retires");stream_clean();
    reset_stream();stream_probe=false;fake.synthetic=false;stream_snapshot.sequence=1;stream_snapshot.origin_generation=1;stream_snapshot.running=stream_snapshot.focused=stream_snapshot.should_render=true;stream_snapshot.head.flags=3;for(unsigned eye=0;eye<2;++eye){auto& e=stream_snapshot.eyes[eye];e.pose.flags=3;e.pose.orientation[3]=1;e.pose.position[0]=eye?.032f:-.032f;const float fov[4]={-.7f,.7f,.7f,-.7f};std::copy(fov,fov+4,e.fov);}
    fake.pose.observed_ns=gpu_scene_now();capture(1);complete(1);stream_snapshot.observed_at_monotonic_ns=gpu_scene_now();xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),15,0xfd029a317a9a4b94ull);
    fake.pose.generation=2;fake.pose.observed_ns=gpu_scene_now();capture(3);complete(3);stream_snapshot.origin_generation=2;stream_snapshot.sequence=2;stream_snapshot.observed_at_monotonic_ns=gpu_scene_now();xr_gpu_eye_stream_network(handle<VkCommandBuffer>(3),35,0xfd029a317a9a4b94ull);xr_gpu_eye_stream_tick(5);check(!xr_gpu_eye_stream_acquire(dev.device,2,eye_stream_now()),"old-origin pending completion is not republished after recenter");fake.completed=35;xr_gpu_eye_stream_tick(6);auto recentered=xr_gpu_eye_stream_acquire(dev.device,2,eye_stream_now());check(recentered&&recentered->target.origin==2,"only new-origin completion becomes visible");recentered.reset();check(xr_gpu_eye_stream_stop(),"recentered output retires");stream_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"recenter leaves no completed resources stranded");
    std::cout<<"Actual continuous GPU eye stream: completed immutable output leases, three-slot backpressure, copied pins, real/synthetic pose scope and uncertain-work retention passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
