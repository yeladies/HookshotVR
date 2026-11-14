// SPDX-License-Identifier: GPL-3.0-or-later
#define XR_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
namespace {
bool mode(const char* value) { const char* s=std::getenv("XR_FAKE_MODE");return s&&!std::strcmp(s,value); }
bool pair_preview(){const char* s=std::getenv("XR_FAKE_MODE");return s&&std::strncmp(s,"preview_pair_",13)==0;}
bool preview_hd(){const char* h=std::getenv("RX_VK_XR_PREVIEW_HD");const char* m=std::getenv("XR_FAKE_MODE");return h&&!std::strcmp(h,"1")&&m&&std::strstr(m,"preview_immersive");}
uint32_t preview_w(){return pair_preview()?8u:preview_hd()?1920u:1280u;}uint32_t preview_h(){return pair_preview()?8u:preview_hd()?1080u:720u;}
std::atomic<int> counts[32]{};
bool (*queue_check)()=nullptr;
int event_stage=0, frames=0;
bool stopping=false, frame_open=false;
XrTime located_time=0;
XrResult XRAPI_CALL create_session(XrInstance,const XrSessionCreateInfo* info,XrSession* out) {
    if(mode("session_create_error"))return XR_ERROR_RUNTIME_FAILURE;
    const auto* binding=static_cast<const XrGraphicsBindingVulkan2KHR*>(info->next);
    if(!binding||binding->instance!=reinterpret_cast<VkInstance>(uintptr_t(2))||
       binding->physicalDevice!=reinterpret_cast<VkPhysicalDevice>(uintptr_t(3))||
       binding->device!=reinterpret_cast<VkDevice>(uintptr_t(4))||binding->queueFamilyIndex!=7||binding->queueIndex!=0)++counts[5];
    *out=reinterpret_cast<XrSession>(uintptr_t(5));++counts[0];event_stage=0;frames=0;stopping=false;frame_open=false;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy_session(XrSession) { ++counts[1];return XR_SUCCESS; }
XrResult XRAPI_CALL create_space(XrSession,const XrReferenceSpaceCreateInfo* info,XrSpace* out) {
    if(mode("session_space_error")&&info->referenceSpaceType==XR_REFERENCE_SPACE_TYPE_VIEW)return XR_ERROR_RUNTIME_FAILURE;
    *out=reinterpret_cast<XrSpace>(uintptr_t(10+counts[6]++));return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy_space(XrSpace) { ++counts[7];return XR_SUCCESS; }
XrResult XRAPI_CALL view_config(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t,uint32_t* count,XrViewConfigurationView*) {
    *count=mode("session_wrong_stereo")?1:2;return XR_SUCCESS;
}
XrResult XRAPI_CALL blends(XrInstance,XrSystemId,XrViewConfigurationType,uint32_t capacity,uint32_t* count,XrEnvironmentBlendMode* out) {
    *count=1;if(capacity)out[0]=mode("session_missing_blend")?XR_ENVIRONMENT_BLEND_MODE_ADDITIVE:XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL poll(XrInstance,XrEventDataBuffer* out) {
    XrSessionState state=XR_SESSION_STATE_UNKNOWN;
    if(stopping) { state=XR_SESSION_STATE_STOPPING;stopping=false;event_stage=99; }
    else if(event_stage==0){state=XR_SESSION_STATE_READY;++event_stage;}
    else if(event_stage==1){state=XR_SESSION_STATE_FOCUSED;++event_stage;}
    else if(frames>=2&&event_stage==2&&mode("session_focus_loss")) {state=XR_SESSION_STATE_VISIBLE;++event_stage;}
    else if(frames>=3&&event_stage==2&&mode("session_instance_loss")) {
        XrEventDataInstanceLossPending event{XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING};
        std::memcpy(out,&event,sizeof event);++event_stage;return XR_SUCCESS;
    }
    else if(frames>=3&&event_stage==2&&(mode("session_stop")||mode("session_loss"))) {
        state=mode("session_stop")?XR_SESSION_STATE_STOPPING:XR_SESSION_STATE_LOSS_PENDING;++event_stage;
    } else if(frames>=1&&event_stage==2&&(mode("session_origin_change")||mode("preview_pair_origin_change")||mode("preview_pair_origin_pending"))) {
        XrEventDataReferenceSpaceChangePending e{XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING};
        e.session=reinterpret_cast<XrSession>(uintptr_t(5));e.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;e.changeTime=pair_preview()?20000000:1000000000000;
        std::memcpy(out,&e,sizeof e);++event_stage;return XR_SUCCESS;
    } else return XR_EVENT_UNAVAILABLE;
    XrEventDataSessionStateChanged e{XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED};
    e.session=reinterpret_cast<XrSession>(uintptr_t(5));e.state=state;std::memcpy(out,&e,sizeof e);return XR_SUCCESS;
}
XrResult XRAPI_CALL begin_session(XrSession,const XrSessionBeginInfo*) { return XR_SUCCESS; }
XrResult XRAPI_CALL end_session(XrSession) { ++counts[8];return XR_SUCCESS; }
XrResult XRAPI_CALL exit_session(XrSession) { ++counts[9];stopping=true;return XR_SUCCESS; }
XrResult XRAPI_CALL wait_frame(XrSession,const XrFrameWaitInfo*,XrFrameState* out) {
    if(mode("session_wait_error"))return XR_ERROR_RUNTIME_FAILURE;
    if(frame_open||(queue_check&&queue_check()))++counts[5];
    std::this_thread::sleep_for(std::chrono::milliseconds(mode("session_stale")&&frames==1?500:5));
    out->predictedDisplayTime=(++frames)*10000000;out->predictedDisplayPeriod=10000000;
    out->shouldRender=mode("session_no_render")?XR_FALSE:XR_TRUE;return XR_SUCCESS;
}
XrResult XRAPI_CALL begin_frame(XrSession,const XrFrameBeginInfo*) {
    if(mode("session_begin_error"))return XR_ERROR_RUNTIME_FAILURE;
    if(frame_open||!queue_check||!queue_check())++counts[5];frame_open=true;++counts[2];return XR_SUCCESS;
}
XrResult XRAPI_CALL end_frame(XrSession,const XrFrameEndInfo* info) {
    if(!queue_check||!queue_check())++counts[5];
        const bool preview_mode=std::getenv("XR_FAKE_MODE")&&std::strncmp(std::getenv("XR_FAKE_MODE"),"preview_",8)==0;
    if(!frame_open||info->displayTime!=frames*10000000||(!preview_mode&&(info->layerCount||info->layers)))++counts[4];
    if(preview_mode&&info->layerCount) {
        if(pair_preview()){
            ++counts[19];const auto* p=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
            if(info->layerCount!=1||p->type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION||p->viewCount!=2||!p->views||p->layerFlags)++counts[5];
            else for(unsigned eye=0;eye<2;++eye){const auto& v=p->views[eye];if(v.type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW||v.subImage.imageRect.extent.width!=8||v.subImage.imageRect.extent.height!=8||v.subImage.imageRect.offset.x!=int(eye*8)||v.subImage.imageArrayIndex||v.pose.orientation.w!=1||std::abs(v.pose.position.x-(eye?10.032f:9.968f))>.0001f||v.pose.position.y!=20||v.pose.position.z!=30||v.fov.angleLeft!=-.72f||v.fov.angleRight!=.78f||v.fov.angleUp!=.88f||v.fov.angleDown!=-.62f)++counts[5];}
        }else if(mode("preview_immersive")||mode("preview_immersive_stereo")||(mode("preview_immersive_transition")&&info->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION)){
            ++counts[19];
            const auto* p=reinterpret_cast<const XrCompositionLayerProjection*>(info->layers[0]);
            if(info->layerCount!=1||p->type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION||p->viewCount!=2||!p->views)++counts[5];
            else for(unsigned eye=0;eye<2;++eye){const auto& v=p->views[eye];
                if(v.type!=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW||v.subImage.imageRect.extent.width!=int32_t(preview_w())||v.subImage.imageRect.extent.height!=int32_t(preview_h())||
                   v.pose.orientation.w!=1||std::abs(v.pose.position.x-(mode("preview_immersive_stereo")?(eye?1.032f:.968f):1.f))>.0001f||v.pose.position.y!=2||v.pose.position.z!=3||v.subImage.imageRect.offset.x!=(mode("preview_immersive_stereo")&&eye?int32_t(preview_w()):0)||v.fov.angleLeft>=0||v.fov.angleRight<=0||v.fov.angleUp<=0||v.fov.angleDown>=0)++counts[5];}
        }else{
            ++counts[20];
            const auto* quad=reinterpret_cast<const XrCompositionLayerQuad*>(info->layers[0]);
            if(info->layerCount!=1||quad->type!=XR_TYPE_COMPOSITION_LAYER_QUAD||quad->eyeVisibility!=XR_EYE_VISIBILITY_BOTH||
               quad->subImage.imageRect.extent.width!=int32_t(preview_w())||quad->size.width!=2.4f||quad->pose.position.x!=1||quad->pose.position.y!=2||quad->pose.position.z!=1)++counts[5];
        }
        ++counts[17];
    }
    frame_open=false;++counts[3];return mode("session_end_error")?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL locate_space(XrSpace,XrSpace,XrTime time,XrSpaceLocation* out) {
    if(mode("session_locate_error"))return XR_ERROR_RUNTIME_FAILURE;
    located_time=time;
    out->pose.orientation.w=1;out->pose.position={1,2,3};
    out->locationFlags=mode("session_invalid_pose")?0:XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    return XR_SUCCESS;
}
XrResult XRAPI_CALL locate_views(XrSession,const XrViewLocateInfo* info,XrViewState* state,uint32_t capacity,uint32_t* count,XrView* views) {
    if(info->displayTime!=located_time||capacity!=2)++counts[5];
    *count=2;state->viewStateFlags=mode("session_invalid_pose")?0:XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    for(int i=0;i<2;++i){views[i].pose.orientation.w=1;views[i].pose.position.x=i?0.032f:-0.032f;if(mode("preview_immersive_stereo")||mode("preview_immersive_stereo_missing")){views[i].pose.position.x+=1;views[i].pose.position.y=2;views[i].pose.position.z=3;}views[i].fov={-0.7f,0.8f,0.9f,-0.6f};}
    return XR_SUCCESS;
}
XrResult XRAPI_CALL extensions(const char*,uint32_t cap,uint32_t* count,XrExtensionProperties* props) {
    *count=mode("no_extension")?0:1;
    if(cap&&*count) std::strcpy(props[0].extensionName,XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
    return XR_SUCCESS;
}
XrResult XRAPI_CALL create(const XrInstanceCreateInfo*,XrInstance* out) {
    *out=reinterpret_cast<XrInstance>(uintptr_t(1));return XR_SUCCESS;
}
XrResult XRAPI_CALL destroy(XrInstance) { return XR_SUCCESS; }
XrResult XRAPI_CALL fake_system(XrInstance,const XrSystemGetInfo*,XrSystemId* out) {
    if(mode("no_headset"))return XR_ERROR_FORM_FACTOR_UNAVAILABLE;
    *out=1;return XR_SUCCESS;
}
XrResult XRAPI_CALL requirements(XrInstance,XrSystemId,XrGraphicsRequirementsVulkanKHR* out) {
    out->minApiVersionSupported=XR_MAKE_VERSION(1,0,0);
    out->maxApiVersionSupported=mode("range")?XR_MAKE_VERSION(1,2,0):XR_MAKE_VERSION(1,3,0);
    return XR_SUCCESS;
}
XrResult XRAPI_CALL instance(XrInstance,const XrVulkanInstanceCreateInfoKHR* info,VkInstance* out,VkResult* result) {
    if(!info->vulkanCreateInfo||info->vulkanCreateInfo->pApplicationInfo->apiVersion!=VK_API_VERSION_1_3)return XR_ERROR_VALIDATION_FAILURE;
    *out=reinterpret_cast<VkInstance>(uintptr_t(2));
    *result=mode("vk_instance_error")?VK_ERROR_INITIALIZATION_FAILED:VK_SUCCESS;
    return mode("xr_instance_error")?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
XrResult XRAPI_CALL physical(XrInstance,const XrVulkanGraphicsDeviceGetInfoKHR*,VkPhysicalDevice* out) {
    *out=reinterpret_cast<VkPhysicalDevice>(uintptr_t(3));return XR_SUCCESS;
}
XrResult XRAPI_CALL device(XrInstance,const XrVulkanDeviceCreateInfoKHR* info,VkDevice* out,VkResult* result) {
    if(info->vulkanPhysicalDevice!=reinterpret_cast<VkPhysicalDevice>(uintptr_t(3)))return XR_ERROR_VALIDATION_FAILURE;
    *out=reinterpret_cast<VkDevice>(uintptr_t(4));
    *result=mode("vk_device_error")?VK_ERROR_FEATURE_NOT_PRESENT:VK_SUCCESS;
    return mode("xr_device_error")?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;
}
}
#include "xr_preview_fake.h"
#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif
extern "C" EXPORT XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out) {
    *out=nullptr;
    if(mode("session_missing_function")&&!std::strcmp(name,"xrWaitFrame"))return XR_ERROR_FUNCTION_UNSUPPORTED;
    if(mode("missing_function")&&!std::strcmp(name,"xrGetSystem"))return XR_ERROR_FUNCTION_UNSUPPORTED;
#define MAP(n,f) if(!std::strcmp(name,n)){*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    MAP("xrEnumerateInstanceExtensionProperties",extensions)
    MAP("xrCreateInstance",create) MAP("xrDestroyInstance",destroy) MAP("xrGetSystem",fake_system)
    MAP("xrGetVulkanGraphicsRequirements2KHR",requirements)
    MAP("xrCreateVulkanInstanceKHR",instance) MAP("xrGetVulkanGraphicsDevice2KHR",physical)
    MAP("xrCreateVulkanDeviceKHR",device)
    MAP("xrCreateSession",create_session) MAP("xrDestroySession",destroy_session)
    MAP("xrCreateReferenceSpace",create_space) MAP("xrDestroySpace",destroy_space)
    MAP("xrEnumerateViewConfigurationViews",view_config) MAP("xrEnumerateEnvironmentBlendModes",blends)
    MAP("xrPollEvent",poll) MAP("xrBeginSession",begin_session) MAP("xrEndSession",end_session)
    MAP("xrRequestExitSession",exit_session) MAP("xrWaitFrame",wait_frame)
    MAP("xrBeginFrame",begin_frame) MAP("xrEndFrame",end_frame)
    MAP("xrLocateSpace",locate_space) MAP("xrLocateViews",locate_views)
    MAP("xrEnumerateSwapchainFormats",preview_formats) MAP("xrCreateSwapchain",preview_create) MAP("xrDestroySwapchain",preview_destroy)
    MAP("xrEnumerateSwapchainImages",preview_images) MAP("xrAcquireSwapchainImage",preview_acquire)
    MAP("xrWaitSwapchainImage",preview_wait) MAP("xrReleaseSwapchainImage",preview_release)
#undef MAP
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
extern "C" EXPORT int XRAPI_CALL xrTestCount(int key) { return key>=0&&key<32?counts[key].load():-1; }

extern "C" EXPORT void XRAPI_CALL xrTestSetQueueCheck(bool (*check)()) { queue_check=check; }

extern "C" EXPORT PFN_vkVoidFunction VKAPI_CALL xrTestVulkanGetInstanceProcAddr(VkInstance instance,const char* name) {return preview_instance_proc(instance,name);}
