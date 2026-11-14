// SPDX-License-Identifier: GPL-3.0-or-later
// Single-owner session: zero-layer probe or an explicit flat-quad transport preview.
#pragma once
#include <array>
#include <algorithm>
#include <chrono>
#include <thread>

namespace mkwvk {
namespace {
struct SessionProbe {
    Bridge& bridge;
    std::mutex& queue_mutex;
    XrSession session=XR_NULL_HANDLE;
    XrSpace local=XR_NULL_HANDLE, head=XR_NULL_HANDLE;
    bool running=false, focused=false, terminal=false;
    uint64_t sequence=0, generation=1, head_samples=0, stereo_samples=0;
    XrTime suppress_until=0;
    PFN_xrDestroySession destroy_session=nullptr;
    PFN_xrDestroySpace destroy_space=nullptr;
    PFN_xrPollEvent poll=nullptr;
    PFN_xrBeginSession begin_session=nullptr;
    PFN_xrEndSession end_session=nullptr;
    PFN_xrRequestExitSession request_exit=nullptr;
    PFN_xrWaitFrame wait_frame=nullptr;
    PFN_xrBeginFrame begin_frame=nullptr;
    PFN_xrEndFrame end_frame=nullptr;
    PFN_xrLocateSpace locate_space=nullptr;
    PFN_xrLocateViews locate_views=nullptr;
    XrFrameSnapshot snapshot{};
    bool visual=false;std::unique_ptr<QuadPreview> preview;std::unique_ptr<GpuPairPreview> gpu_preview;
    explicit SessionProbe(Bridge& owner,std::mutex& queue,bool show):bridge(owner),queue_mutex(queue),visual(show) { generation=++bridge.origin_epoch; }
    bool initialize(VkInstance instance,VkPhysicalDevice physical,VkDevice device,uint32_t family) {
        PFN_xrCreateSession create=nullptr;
        PFN_xrCreateReferenceSpace create_space=nullptr;
        PFN_xrEnumerateViewConfigurationViews view_config=nullptr;
        PFN_xrEnumerateEnvironmentBlendModes blend_modes=nullptr;
#define LOAD(n,f) if(!bridge.function(n,f)) return false
        LOAD("xrCreateSession",create); LOAD("xrDestroySession",destroy_session);
        LOAD("xrCreateReferenceSpace",create_space); LOAD("xrDestroySpace",destroy_space);
        LOAD("xrEnumerateViewConfigurationViews",view_config); LOAD("xrEnumerateEnvironmentBlendModes",blend_modes);
        LOAD("xrPollEvent",poll); LOAD("xrBeginSession",begin_session); LOAD("xrEndSession",end_session);
        LOAD("xrRequestExitSession",request_exit); LOAD("xrWaitFrame",wait_frame); LOAD("xrBeginFrame",begin_frame);
        LOAD("xrEndFrame",end_frame); LOAD("xrLocateSpace",locate_space); LOAD("xrLocateViews",locate_views);
#undef LOAD
        uint32_t count=0;
        if(!ok(view_config(bridge.instance,bridge.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"stereo count")||count!=2) return false;
        if(!ok(blend_modes(bridge.instance,bridge.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"blend count")) return false;
        std::vector<XrEnvironmentBlendMode> blends(count);
        if(!ok(blend_modes(bridge.instance,bridge.system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,blends.data()),"blend modes")||
           std::find(blends.begin(),blends.end(),XR_ENVIRONMENT_BLEND_MODE_OPAQUE)==blends.end()) return false;
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance=instance;binding.physicalDevice=physical;binding.device=device;binding.queueFamilyIndex=family;binding.queueIndex=0;
        XrSessionCreateInfo info{XR_TYPE_SESSION_CREATE_INFO};info.next=&binding;info.systemId=bridge.system;
        if(!ok(create(bridge.instance,&info,&session),"create renderer session")) return false;
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};space.poseInReferenceSpace.orientation.w=1;
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        if(!ok(create_space(session,&space,&local),"create LOCAL space"))return false;
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        if(!ok(create_space(session,&space,&head),"create VIEW space"))return false;
        const char* gpu_mode=std::getenv("RX_VK_XR_GPU_WORLD_PREVIEW");
        if(visual&&gpu_mode&&!std::strcmp(gpu_mode,"1")){
            gpu_preview=std::make_unique<GpuPairPreview>(bridge,queue_mutex);
            if(!gpu_preview->initialize(session,instance,physical,device,family))return false;
            bridge.preview_accepting.store(false);
        }else if(visual) {
            preview=std::make_unique<QuadPreview>(bridge,queue_mutex);
            if(!preview->initialize(session,instance,physical,device,family))return false;
            bridge.preview_accepting.store(true);
        }
        logf("xr: renderer session created; %s, no native game stereo",gpu_preview?"GPU reconstructed stereo projection":visual?"flat quad preview":"zero-layer probe");
        return true;
    }
    bool events() {
        XrEventDataBuffer data{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult result;
        while((result=poll(bridge.instance,&data))==XR_SUCCESS) {
            if(data.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto& event=*reinterpret_cast<XrEventDataSessionStateChanged*>(&data);
                if(event.session==session) {
                    logf("xr: renderer session state %d",int(event.state));
                    focused=event.state==XR_SESSION_STATE_FOCUSED;
                    if(event.state==XR_SESSION_STATE_READY&&!running) {
                        XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        if(!ok(begin_session(session,&begin),"begin renderer session"))return false;
                        running=true;
                    } else if(event.state==XR_SESSION_STATE_STOPPING) {
                        if(running&&!ok(end_session(session),"end renderer session"))return false;
                        running=false;terminal=true;
                    } else if(event.state==XR_SESSION_STATE_EXITING||event.state==XR_SESSION_STATE_LOSS_PENDING) {
                        terminal=true;
                    }
                }
            } else if(data.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                terminal=true;
            } else if(data.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) {
                const auto& event=*reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&data);
                if(event.session==session&&event.referenceSpaceType==XR_REFERENCE_SPACE_TYPE_LOCAL) {
                    generation=++bridge.origin_epoch;suppress_until=std::max(suppress_until,event.changeTime);
                    snapshot={}; // A new origin notification invalidates the previous sample immediately.
                }
            }
            data={XR_TYPE_EVENT_DATA_BUFFER};
        }
        return result==XR_EVENT_UNAVAILABLE||ok(result,"poll renderer events");
    }
    static void pose(XrTrackedPose& out,const XrPosef& in,uint64_t flags) {
        out={{in.orientation.x,in.orientation.y,in.orientation.z,in.orientation.w},
             {in.position.x,in.position.y,in.position.z},flags};
    }
    bool tick() {
        if(!events()||terminal)return false;
        if(!running) { snapshot={};return true; }
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState frame{XR_TYPE_FRAME_STATE};
        if(!ok(wait_frame(session,&wait,&frame),"wait renderer frame"))return false;
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        XrResult begun;
        { std::lock_guard lock(queue_mutex);begun=begin_frame(session,&begin); }
        if(!ok(begun,"begin renderer frame"))return false;
        XrFrameSnapshot candidate{};
        candidate.sequence=++sequence;candidate.origin_generation=generation;
        candidate.predicted_display_time=frame.predictedDisplayTime;
        candidate.running=true;candidate.focused=focused;candidate.should_render=frame.shouldRender;
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime=frame.predictedDisplayTime;locate.space=local;
        XrViewState state{XR_TYPE_VIEW_STATE};uint32_t count=0;
        std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        const bool located=ok(locate_space(head,local,frame.predictedDisplayTime,&location),"locate renderer head")&&
            ok(locate_views(session,&locate,&state,2,&count,views.data()),"locate renderer views")&&count==2;
        bool quad=false,draw_ok=true;
        constexpr auto needed=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if(preview&&located&&frame.shouldRender&&frame.predictedDisplayTime>=suppress_until&&(location.locationFlags&needed)==needed) {
            const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            auto image=bridge.preview_feed.latest(now);
            if(image.pixels&&(!preview->immersive||((image.immersive.valid||image.immersive.menu)&&image.immersive.generation==generation))){draw_ok=preview->frame(local,location.pose,generation,std::move(image));quad=draw_ok;}
        }
        if(gpu_preview&&located&&frame.shouldRender&&frame.predictedDisplayTime>=suppress_until&&(location.locationFlags&needed)==needed){draw_ok=gpu_preview->frame(local,generation);quad=draw_ok&&gpu_preview->submitted_layer();}
        // Balance every begun frame, including failures and no-image/not-visible frames.
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frame.predictedDisplayTime;
        end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        const XrCompositionLayerBaseHeader* layers[1]{};
        if(quad){layers[0]=gpu_preview?gpu_preview->submitted_layer():preview->submitted_layer();end.layerCount=1;end.layers=layers;}
        XrResult ended;
        { std::lock_guard lock(queue_mutex);ended=end_frame(session,&end); }
        if(!ok(ended,"end renderer frame")||!located||!draw_ok)return false;
        if(quad&&gpu_preview)gpu_preview->submitted();
        if(frame.predictedDisplayTime>=suppress_until) {
            pose(candidate.head,location.pose,location.locationFlags);
            uint64_t flags=0;
            if(state.viewStateFlags&XR_VIEW_STATE_POSITION_VALID_BIT)flags|=XR_SPACE_LOCATION_POSITION_VALID_BIT;
            if(state.viewStateFlags&XR_VIEW_STATE_ORIENTATION_VALID_BIT)flags|=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if(state.viewStateFlags&XR_VIEW_STATE_POSITION_TRACKED_BIT)flags|=XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
            if(state.viewStateFlags&XR_VIEW_STATE_ORIENTATION_TRACKED_BIT)flags|=XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
            for(int i=0;i<2;++i) {
                pose(candidate.eyes[i].pose,views[i].pose,flags);
                const auto f=views[i].fov;candidate.eyes[i].fov[0]=f.angleLeft;candidate.eyes[i].fov[1]=f.angleRight;
                candidate.eyes[i].fov[2]=f.angleUp;candidate.eyes[i].fov[3]=f.angleDown;
            }
            constexpr auto need=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
            if((candidate.head.flags&need)==need)++head_samples;
            if((flags&need)==need)++stereo_samples;
        }
        candidate.observed_at_monotonic_ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        snapshot=candidate;
        if(sequence==1||sequence%120==0)logf("xr: %s frames %llu, valid head %llu, valid stereo poses %llu",
            visual?"flat-preview session":"zero-layer",
            (unsigned long long)sequence,(unsigned long long)head_samples,(unsigned long long)stereo_samples);
        return true;
    }
    void close() {
        bridge.preview_accepting.store(false);bridge.preview_feed.clear();
        if(session&&running&&!terminal) {
            if(ok(request_exit(session),"request renderer session exit")) {
                // Poll for STOPPING for a bounded time; destruction is legal even if the runtime never delivers it.
                for(int i=0;i<50&&running&&!terminal;++i) {
                    if(!events())break;
                    if(running)std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        if(gpu_preview&&!gpu_preview->close()){
            bridge.retained_gpu_work=true;bridge.ready=false;gpu_preview->report("retained");gpu_preview.release();logf("xr-gpu: uncertain GPU/image ownership; retaining session/runtime and source lease until process exit");return;
        }
        gpu_preview.reset();
        if(preview&&!preview->close()) {
            bridge.retained_gpu_work=true;bridge.ready=false;preview.release();
            logf("xr: preview GPU completion failed; retaining session/runtime resources until process exit");return;
        }
        preview.reset();
        if(head&&destroy_space)ok(destroy_space(head),"destroy VIEW space");head=XR_NULL_HANDLE;
        if(local&&destroy_space)ok(destroy_space(local),"destroy LOCAL space");local=XR_NULL_HANDLE;
        if(session&&destroy_session)ok(destroy_session(session),"destroy renderer session");session=XR_NULL_HANDLE;
        running=false;snapshot={};
        logf("xr: renderer session probe closed; frames %llu, valid head %llu, valid stereo poses %llu",
            (unsigned long long)sequence,(unsigned long long)head_samples,(unsigned long long)stereo_samples);
    }
};
}
}