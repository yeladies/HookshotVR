// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal stereo marker scene: no game, shaders, assets or simulated physics.
#define VK_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "scene_math.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
void xr_check(XrResult r,const char* name) {
    if(XR_FAILED(r)) throw std::runtime_error(std::string(name)+": XrResult "+std::to_string(r));
}
void vk_check(VkResult r,const char* name) {
    if(r!=VK_SUCCESS) throw std::runtime_error(std::string(name)+": VkResult "+std::to_string(r));
}
template<class T> T xr_function(XrInstance instance,const char* name) {
    PFN_xrVoidFunction p=nullptr;
    xr_check(xrGetInstanceProcAddr(instance,name,&p),name);
    return reinterpret_cast<T>(p);
}
#define DEVICE_FUNCTIONS(X) \
 X(DestroyDevice) X(GetDeviceQueue) X(CreateCommandPool) X(DestroyCommandPool) X(AllocateCommandBuffers) \
 X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer) X(CreateFence) X(DestroyFence) \
 X(ResetFences) X(WaitForFences) X(QueueSubmit) X(DeviceWaitIdle) X(CreateRenderPass) X(DestroyRenderPass) \
 X(CreateImageView) X(DestroyImageView) X(CreateFramebuffer) X(DestroyFramebuffer) \
 X(CmdBeginRenderPass) X(CmdEndRenderPass) X(CmdClearAttachments)
struct Vulkan {
    VkInstance instance=VK_NULL_HANDLE;
    VkPhysicalDevice physical=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkQueue queue=VK_NULL_HANDLE;
    uint32_t queue_family=0;
    PFN_vkGetInstanceProcAddr get=nullptr;
    PFN_vkGetDeviceProcAddr get_device=nullptr;
    PFN_vkDestroyInstance destroy_instance=nullptr;
#define DECL(name) PFN_vk##name name=nullptr;
    DEVICE_FUNCTIONS(DECL)
#undef DECL
#ifdef _WIN32
    HMODULE library=nullptr;
#else
    void* library=nullptr;
#endif
    void load() {
#ifdef _WIN32
        library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library) get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
#else
        library=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
        if(library) get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library,"vkGetInstanceProcAddr"));
#endif
        if(!get) throw std::runtime_error("System Vulkan loader unavailable");
    }
    template<class T> T function(const char* name) {
        const auto p=get(instance,name);
        if(!p) throw std::runtime_error(std::string("Missing Vulkan function: ")+name);
        return reinterpret_cast<T>(p);
    }
    void device_functions() {
        get_device=function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
#define LOAD(name) name=reinterpret_cast<PFN_vk##name>(get_device(device,"vk" #name)); if(!name) throw std::runtime_error("Missing vk" #name);
        DEVICE_FUNCTIONS(LOAD)
#undef LOAD
    }
    ~Vulkan() {
        if(device && DestroyDevice) DestroyDevice(device,nullptr);
        if(instance && destroy_instance) destroy_instance(instance,nullptr);
#ifdef _WIN32
        if(library) FreeLibrary(library);
#else
        if(library) dlclose(library);
#endif
    }
};
struct Eye {
    XrSwapchain swapchain=XR_NULL_HANDLE;
    uint32_t width=0,height=0;
    std::vector<XrSwapchainImageVulkan2KHR> images;
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> frames;
};
struct Marker { XrVector3f position; float radius; std::array<float,4> color; };
struct Scene {
    Vulkan vk;
    XrInstance instance=XR_NULL_HANDLE;
    XrSystemId system=XR_NULL_SYSTEM_ID;
    XrSession session=XR_NULL_HANDLE;
    XrSpace local=XR_NULL_HANDLE, head=XR_NULL_HANDLE;
    XrActionSet action_set=XR_NULL_HANDLE;
    XrAction grip=XR_NULL_HANDLE;
    std::array<XrPath,2> hands{};
    std::array<XrSpace,2> hand_spaces{};
    std::array<XrSpaceLocation,2> hand_locations{};
    std::array<bool,2> hand_valid{};
    std::array<Eye,2> eyes{};
    VkCommandPool pool=VK_NULL_HANDLE;
    VkCommandBuffer command=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    VkRenderPass render_pass=VK_NULL_HANDLE;
    bool running=false,focused=false,exiting=false;
    uint64_t frames=0,rendered=0,head_samples=0,view_samples=0;
    std::array<uint64_t,2> hand_samples{};
    XrPosef last_head{};
    std::array<XrPosef,2> last_hands{};
    XrVector3f minimum{},maximum{};
    bool have_position=false;
    std::string gpu;
    XrPath path(const char* value) { XrPath p;xr_check(xrStringToPath(instance,value,&p),"xrStringToPath");return p; }
    void init() {
        uint32_t count=0;
        xr_check(xrEnumerateInstanceExtensionProperties(nullptr,0,&count,nullptr),"xrEnumerateInstanceExtensionProperties");
        std::vector<XrExtensionProperties> ext(count,{XR_TYPE_EXTENSION_PROPERTIES});
        xr_check(xrEnumerateInstanceExtensionProperties(nullptr,count,&count,ext.data()),"xrEnumerateInstanceExtensionProperties");
        if(std::none_of(ext.begin(),ext.end(),[](auto& p){return !std::strcmp(p.extensionName,XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);}))
            throw std::runtime_error("Runtime does not support XR_KHR_vulkan_enable2");
        const char* extensions[]={XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME};
        XrInstanceCreateInfo info{XR_TYPE_INSTANCE_CREATE_INFO};
        std::strcpy(info.applicationInfo.applicationName,"HookshotVR Spatial Test");
        info.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        info.enabledExtensionCount=1;info.enabledExtensionNames=extensions;
        xr_check(xrCreateInstance(&info,&instance),"xrCreateInstance");
        XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};si.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        xr_check(xrGetSystem(instance,&si,&system),"xrGetSystem");
        init_vulkan();
        XrGraphicsBindingVulkan2KHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN2_KHR};
        binding.instance=vk.instance;binding.physicalDevice=vk.physical;binding.device=vk.device;
        binding.queueFamilyIndex=vk.queue_family;binding.queueIndex=0;
        XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};sci.next=&binding;sci.systemId=system;
        xr_check(xrCreateSession(instance,&sci,&session),"xrCreateSession");
        XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        space.poseInReferenceSpace.orientation.w=1;space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;
        xr_check(xrCreateReferenceSpace(session,&space,&local),"create local space");
        space.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;
        xr_check(xrCreateReferenceSpace(session,&space,&head),"create head space");
        init_actions();
        init_images();
    }
    void init_vulkan() {
        const auto requirements=xr_function<PFN_xrGetVulkanGraphicsRequirements2KHR>(instance,"xrGetVulkanGraphicsRequirements2KHR");
        XrGraphicsRequirementsVulkan2KHR req{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN2_KHR};
        xr_check(requirements(instance,system,&req),"xrGetVulkanGraphicsRequirements2KHR");
        const auto api=XR_MAKE_VERSION(1,3,0);
        if(req.minApiVersionSupported>api || req.maxApiVersionSupported<api) throw std::runtime_error("Runtime does not allow Vulkan 1.3");
        vk.load();
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="HookshotVR Spatial Test";app.apiVersion=VK_API_VERSION_1_3;
        VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
        XrVulkanInstanceCreateInfoKHR xci{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
        xci.systemId=system;xci.pfnGetInstanceProcAddr=vk.get;xci.vulkanCreateInfo=&ci;
        VkResult result=VK_SUCCESS;
        xr_check(xr_function<PFN_xrCreateVulkanInstanceKHR>(instance,"xrCreateVulkanInstanceKHR")(instance,&xci,&vk.instance,&result),"xrCreateVulkanInstanceKHR");
        vk_check(result,"create Vulkan instance");
        vk.destroy_instance=vk.function<PFN_vkDestroyInstance>("vkDestroyInstance");
        XrVulkanGraphicsDeviceGetInfoKHR get_info{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
        get_info.systemId=system;get_info.vulkanInstance=vk.instance;
        xr_check(xr_function<PFN_xrGetVulkanGraphicsDevice2KHR>(instance,"xrGetVulkanGraphicsDevice2KHR")(instance,&get_info,&vk.physical),"xrGetVulkanGraphicsDevice2KHR");
        VkPhysicalDeviceProperties props{};
        vk.function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(vk.physical,&props);
        gpu=props.deviceName;
        if(props.apiVersion<VK_API_VERSION_1_3) throw std::runtime_error("Selected GPU lacks Vulkan 1.3");
        const auto queue_props=vk.function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
        uint32_t n=0;queue_props(vk.physical,&n,nullptr);
        std::vector<VkQueueFamilyProperties> families(n);queue_props(vk.physical,&n,families.data());
        auto family=std::find_if(families.begin(),families.end(),[](auto& f){return f.queueCount && (f.queueFlags&VK_QUEUE_GRAPHICS_BIT);});
        if(family==families.end()) throw std::runtime_error("GPU has no graphics queue");
        vk.queue_family=static_cast<uint32_t>(family-families.begin());
        float priority=1;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=vk.queue_family;queue.queueCount=1;queue.pQueuePriorities=&priority;
        VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;
        XrVulkanDeviceCreateInfoKHR create{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
        create.systemId=system;create.pfnGetInstanceProcAddr=vk.get;create.vulkanPhysicalDevice=vk.physical;create.vulkanCreateInfo=&device;
        xr_check(xr_function<PFN_xrCreateVulkanDeviceKHR>(instance,"xrCreateVulkanDeviceKHR")(instance,&create,&vk.device,&result),"xrCreateVulkanDeviceKHR");
        vk_check(result,"create Vulkan device");vk.device_functions();
        vk.GetDeviceQueue(vk.device,vk.queue_family,0,&vk.queue);
        VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool_info.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pool_info.queueFamilyIndex=vk.queue_family;
        vk_check(vk.CreateCommandPool(vk.device,&pool_info,nullptr,&pool),"vkCreateCommandPool");
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocate.commandPool=pool;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;
        vk_check(vk.AllocateCommandBuffers(vk.device,&allocate,&command),"vkAllocateCommandBuffers");
        VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        vk_check(vk.CreateFence(vk.device,&fence_info,nullptr,&fence),"vkCreateFence");
    }
    void init_actions() {
        hands={path("/user/hand/left"),path("/user/hand/right")};
        XrActionSetCreateInfo set{XR_TYPE_ACTION_SET_CREATE_INFO};
        std::strcpy(set.actionSetName,"spatial_test");std::strcpy(set.localizedActionSetName,"Spatial test");
        xr_check(xrCreateActionSet(instance,&set,&action_set),"xrCreateActionSet");
        XrActionCreateInfo action{XR_TYPE_ACTION_CREATE_INFO};action.actionType=XR_ACTION_TYPE_POSE_INPUT;
        action.countSubactionPaths=2;action.subactionPaths=hands.data();
        std::strcpy(action.actionName,"grip_pose");std::strcpy(action.localizedActionName,"Controller grip");
        xr_check(xrCreateAction(action_set,&action,&grip),"xrCreateAction");
        std::array<XrActionSuggestedBinding,2> bindings{{{grip,path("/user/hand/left/input/grip/pose")},{grip,path("/user/hand/right/input/grip/pose")}}};
        XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
        suggested.interactionProfile=path("/interaction_profiles/oculus/touch_controller");
        suggested.countSuggestedBindings=2;suggested.suggestedBindings=bindings.data();
        xr_check(xrSuggestInteractionProfileBindings(instance,&suggested),"xrSuggestInteractionProfileBindings");
        XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&action_set;
        xr_check(xrAttachSessionActionSets(session,&attach),"xrAttachSessionActionSets");
        for(size_t i=0;i<2;++i) {
            XrActionSpaceCreateInfo s{XR_TYPE_ACTION_SPACE_CREATE_INFO};s.action=grip;s.subactionPath=hands[i];s.poseInActionSpace.orientation.w=1;
            xr_check(xrCreateActionSpace(session,&s,&hand_spaces[i]),"xrCreateActionSpace");
        }
    }
    void init_images() {
        uint32_t count=0;xr_check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr),"enumerate stereo views");
        if(count!=2) throw std::runtime_error("Exactly two stereo views are required");
        std::array<XrViewConfigurationView,2> config{{{XR_TYPE_VIEW_CONFIGURATION_VIEW},{XR_TYPE_VIEW_CONFIGURATION_VIEW}}};
        xr_check(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,2,&count,config.data()),"enumerate stereo views");
        xr_check(xrEnumerateSwapchainFormats(session,0,&count,nullptr),"enumerate formats");
        std::vector<int64_t> formats(count);xr_check(xrEnumerateSwapchainFormats(session,count,&count,formats.data()),"enumerate formats");
        VkFormat format=VK_FORMAT_UNDEFINED;
        for(auto preferred:{VK_FORMAT_R8G8B8A8_SRGB,VK_FORMAT_B8G8R8A8_SRGB,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_B8G8R8A8_UNORM})
            if(std::find(formats.begin(),formats.end(),preferred)!=formats.end()) {format=preferred;break;}
        if(format==VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported RGBA8 swapchain format");
        VkAttachmentDescription attachment{};attachment.format=format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference reference{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&reference;
        VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};pass.attachmentCount=1;pass.pAttachments=&attachment;pass.subpassCount=1;pass.pSubpasses=&subpass;
        vk_check(vk.CreateRenderPass(vk.device,&pass,nullptr,&render_pass),"vkCreateRenderPass");
        for(size_t i=0;i<2;++i) {
            auto& eye=eyes[i];
            eye.width=std::min(1200u,config[i].recommendedImageRectWidth);eye.height=std::min(1300u,config[i].recommendedImageRectHeight);
            if(!eye.width || !eye.height) throw std::runtime_error("Invalid eye size");
            XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};ci.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            ci.format=format;ci.sampleCount=1;ci.width=eye.width;ci.height=eye.height;ci.faceCount=1;ci.arraySize=1;ci.mipCount=1;
            xr_check(xrCreateSwapchain(session,&ci,&eye.swapchain),"xrCreateSwapchain");
            xr_check(xrEnumerateSwapchainImages(eye.swapchain,0,&count,nullptr),"count swapchain images");
            eye.images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
            xr_check(xrEnumerateSwapchainImages(eye.swapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())),"enumerate swapchain images");
            eye.views.resize(count);eye.frames.resize(count);
            for(uint32_t j=0;j<count;++j) {
                VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=eye.images[j].image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;
                view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
                vk_check(vk.CreateImageView(vk.device,&view,nullptr,&eye.views[j]),"vkCreateImageView");
                VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};framebuffer.renderPass=render_pass;framebuffer.attachmentCount=1;framebuffer.pAttachments=&eye.views[j];
                framebuffer.width=eye.width;framebuffer.height=eye.height;framebuffer.layers=1;
                vk_check(vk.CreateFramebuffer(vk.device,&framebuffer,nullptr,&eye.frames[j]),"vkCreateFramebuffer");
            }
        }
    }
    void events() {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        XrResult r;
        while((r=xrPollEvent(instance,&event))==XR_SUCCESS) {
            if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                auto state=reinterpret_cast<XrEventDataSessionStateChanged*>(&event)->state;
                focused=state==XR_SESSION_STATE_FOCUSED;
                std::cerr<<"Session state "<<int(state)<<'\n';
                if(state==XR_SESSION_STATE_READY && !running) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xr_check(xrBeginSession(session,&begin),"xrBeginSession");running=true;
                } else if(state==XR_SESSION_STATE_STOPPING && running) {
                    xr_check(xrEndSession(session),"xrEndSession");running=false;exiting=true;
                } else if(state==XR_SESSION_STATE_EXITING || state==XR_SESSION_STATE_LOSS_PENDING) exiting=true;
            } else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) exiting=true;
            event={XR_TYPE_EVENT_DATA_BUFFER};
        }
        if(r!=XR_EVENT_UNAVAILABLE) xr_check(r,"xrPollEvent");
    }
    void poses(XrTime time) {
        XrSpaceLocation position{XR_TYPE_SPACE_LOCATION};
        xr_check(xrLocateSpace(head,local,time,&position),"locate head");
        if(hookshot::vr::pose_valid(position.locationFlags)) {
            ++head_samples;
            last_head=position.pose;
            const auto p=position.pose.position;
            if(!have_position) {minimum=maximum=p;have_position=true;}
            minimum={std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
            maximum={std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
        }
        hand_valid={false,false};
        if(!focused) return;
        XrActiveActionSet active{action_set,XR_NULL_PATH};
        XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
        const XrResult result=xrSyncActions(session,&sync);
        if(result==XR_SESSION_NOT_FOCUSED) return;
        xr_check(result,"xrSyncActions");
        for(size_t i=0;i<2;++i) {
            XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=grip;get.subactionPath=hands[i];
            XrActionStatePose pose{XR_TYPE_ACTION_STATE_POSE};xr_check(xrGetActionStatePose(session,&get,&pose),"xrGetActionStatePose");
            if(!pose.isActive) continue;
            hand_locations[i]={XR_TYPE_SPACE_LOCATION};xr_check(xrLocateSpace(hand_spaces[i],local,time,&hand_locations[i]),"locate hand");
            hand_valid[i]=hookshot::vr::pose_valid(hand_locations[i].locationFlags);
            if(hand_valid[i]) {++hand_samples[i];last_hands[i]=hand_locations[i].pose;}
        }
    }
    void draw(Eye& eye,uint32_t image,const XrView& view) {
        vk_check(vk.ResetCommandBuffer(command,0),"vkResetCommandBuffer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vk_check(vk.BeginCommandBuffer(command,&begin),"vkBeginCommandBuffer");
        VkClearValue background{};background.color={{0.015f,0.025f,0.05f,1}};
        VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};pass.renderPass=render_pass;pass.framebuffer=eye.frames.at(image);
        pass.renderArea={{0,0},{eye.width,eye.height}};pass.clearValueCount=1;pass.pClearValues=&background;
        vk.CmdBeginRenderPass(command,&pass,VK_SUBPASS_CONTENTS_INLINE);
        std::vector<Marker> markers;
        for(int x=-4;x<=4;++x) for(int z=1;z<=9;++z) markers.push_back({{x*0.4f,-1.2f,-z*0.4f},0.018f,{0.07f,0.25f,0.32f,1}});
        markers.push_back({{-0.5f,0,-2},0.09f,{0.9f,0.08f,0.05f,1}});
        markers.push_back({{0,0,-2},0.09f,{0.08f,0.85f,0.15f,1}});
        markers.push_back({{0.5f,0,-2},0.09f,{0.1f,0.3f,1,1}});
        for(size_t i=0;i<2;++i) if(hand_valid[i]) markers.push_back({hand_locations[i].pose.position,0.035f,i==0?std::array<float,4>{1,0.4f,0.05f,1}:std::array<float,4>{0.1f,0.9f,1,1}});
        struct Painted {hookshot::vr::MarkerRect rect;std::array<float,4> color;};
        std::vector<Painted> visible;
        for(const auto& marker:markers) {
            hookshot::vr::MarkerRect rect{};
            if(hookshot::vr::marker_rect(view.pose,view.fov,marker.position,marker.radius,eye.width,eye.height,rect)) visible.push_back({rect,marker.color});
        }
        std::sort(visible.begin(),visible.end(),[](auto& a,auto& b){return a.rect.distance>b.rect.distance;});
        for(const auto& p:visible) {
            VkClearAttachment clear{};clear.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
            std::copy(p.color.begin(),p.color.end(),clear.clearValue.color.float32);
            VkClearRect rect{{{p.rect.x,p.rect.y},{p.rect.width,p.rect.height}},0,1};
            vk.CmdClearAttachments(command,1,&clear,1,&rect);
        }
        vk.CmdEndRenderPass(command);vk_check(vk.EndCommandBuffer(command),"vkEndCommandBuffer");
        vk_check(vk.ResetFences(vk.device,1,&fence),"vkResetFences");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
        vk_check(vk.QueueSubmit(vk.queue,1,&submit,fence),"vkQueueSubmit");
        // Deliberately synchronous first test; do not release runtime images before GPU completion.
        vk_check(vk.WaitForFences(vk.device,1,&fence,VK_TRUE,5000000000ull),"vkWaitForFences");
    }
    void frame() {
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};XrFrameState state{XR_TYPE_FRAME_STATE};
        xr_check(xrWaitFrame(session,&wait,&state),"xrWaitFrame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};xr_check(xrBeginFrame(session,&begin),"xrBeginFrame");
        ++frames;
        poses(state.predictedDisplayTime);
        XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate.displayTime=state.predictedDisplayTime;locate.space=local;
        XrViewState view_state{XR_TYPE_VIEW_STATE};uint32_t n=0;
        std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
        xr_check(xrLocateViews(session,&locate,&view_state,2,&n,views.data()),"xrLocateViews");
        const bool valid=n==2 && (view_state.viewStateFlags&(XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT))==(XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT);
        if(valid) ++view_samples;
        std::array<XrCompositionLayerProjectionView,2> projection{{{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW},{XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}}};
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=local;layer.viewCount=2;layer.views=projection.data();
        const XrCompositionLayerBaseHeader* layers[]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer)};
        if(state.shouldRender && valid) {
            for(size_t i=0;i<2;++i) {
                auto& eye=eyes[i];uint32_t index=0;
                XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};xr_check(xrAcquireSwapchainImage(eye.swapchain,&acquire,&index),"xrAcquireSwapchainImage");
                XrSwapchainImageWaitInfo image_wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};image_wait.timeout=XR_INFINITE_DURATION;
                xr_check(xrWaitSwapchainImage(eye.swapchain,&image_wait),"xrWaitSwapchainImage");
                draw(eye,index,views[i]);
                XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xr_check(xrReleaseSwapchainImage(eye.swapchain,&release),"xrReleaseSwapchainImage");
                projection[i].pose=views[i].pose;projection[i].fov=views[i].fov;
                projection[i].subImage={eye.swapchain,{{0,0},{static_cast<int32_t>(eye.width),static_cast<int32_t>(eye.height)}},0};
            }
            ++rendered;
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=state.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        if(state.shouldRender && valid) {end.layerCount=1;end.layers=layers;}
        xr_check(xrEndFrame(session,&end),"xrEndFrame");
    }
    void run(int seconds) {
        const auto started=Clock::now();auto deadline=started+std::chrono::seconds(45);
        bool began=false,visible_once=false,requested_exit=false;
        while(!exiting) {
            events();if(exiting) break;
            const auto now=Clock::now();
            if(running) began=true;
            if(rendered && !visible_once) {visible_once=true;deadline=now+std::chrono::seconds(seconds);}
            if(now>=deadline) {
                if(!began) throw std::runtime_error("Session never reached READY");
                if(!visible_once) std::cerr<<"Runtime did not make the scene visible within 45 seconds. No frames were forced while shouldRender was false.\n";
                if(!requested_exit && running) {xr_check(xrRequestExitSession(session),"xrRequestExitSession");requested_exit=true;deadline=now+std::chrono::seconds(2);}
                else break;
            }
            if(running) frame();else std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    void report() const {
        auto pose=[](const XrPosef& p) {
            std::cout<<"{\"position_m\":["<<p.position.x<<','<<p.position.y<<','<<p.position.z<<"],\"orientation_xyzw\":["
                     <<p.orientation.x<<','<<p.orientation.y<<','<<p.orientation.z<<','<<p.orientation.w<<"]}";
        };
        std::cout<<"{\n  \"session_created\": "<<(session?"true":"false")<<",\n  \"frames\": "<<frames
                 <<",\n  \"stereo_frames_submitted\": "<<rendered<<",\n  \"valid_view_samples\": "<<view_samples
                 <<",\n  \"valid_head_samples\": "<<head_samples<<",\n  \"left_hand_samples\": "<<hand_samples[0]
                 <<",\n  \"right_hand_samples\": "<<hand_samples[1]
                 <<",\n  \"head_position_range_m\": ["<<maximum.x-minimum.x<<','<<maximum.y-minimum.y<<','<<maximum.z-minimum.z<<"]"
                 <<",\n  \"last_head_pose\": ";
        if(head_samples) pose(last_head);else std::cout<<"null";
        std::cout<<",\n  \"last_left_hand_pose\": ";if(hand_samples[0]) pose(last_hands[0]);else std::cout<<"null";
        std::cout<<",\n  \"last_right_hand_pose\": ";if(hand_samples[1]) pose(last_hands[1]);else std::cout<<"null";
        std::cout<<"\n}\n";
        std::cerr<<"Runtime-selected GPU: "<<gpu<<'\n';
    }
    ~Scene() {
        if(vk.device && vk.DeviceWaitIdle) vk.DeviceWaitIdle(vk.device);
        for(auto& eye:eyes) {
            for(auto f:eye.frames) if(f && vk.DestroyFramebuffer) vk.DestroyFramebuffer(vk.device,f,nullptr);
            for(auto v:eye.views) if(v && vk.DestroyImageView) vk.DestroyImageView(vk.device,v,nullptr);
            if(eye.swapchain) xrDestroySwapchain(eye.swapchain);
        }
        for(auto s:hand_spaces) if(s) xrDestroySpace(s);
        if(head) xrDestroySpace(head);
        if(local) xrDestroySpace(local);
        if(session) xrDestroySession(session);
        if(action_set) xrDestroyActionSet(action_set);
        if(vk.device) {
            if(render_pass && vk.DestroyRenderPass) vk.DestroyRenderPass(vk.device,render_pass,nullptr);
            if(fence && vk.DestroyFence) vk.DestroyFence(vk.device,fence,nullptr);
            if(pool && vk.DestroyCommandPool) vk.DestroyCommandPool(vk.device,pool,nullptr);
            if(vk.DestroyDevice) {vk.DestroyDevice(vk.device,nullptr);vk.device=VK_NULL_HANDLE;}
        }
        if(vk.instance && vk.destroy_instance) {vk.destroy_instance(vk.instance,nullptr);vk.instance=VK_NULL_HANDLE;}
        if(instance) xrDestroyInstance(instance);
    }
};
} // namespace
int main(int argc,char** argv) {
    int seconds=15;
    if(argc==3 && std::string(argv[1])=="--seconds") {
        try {size_t used=0;seconds=std::stoi(argv[2],&used);if(used!=std::strlen(argv[2]) || seconds<1 || seconds>90) throw std::runtime_error("range");}
        catch(...) {std::cerr<<"--seconds must be 1..90\n";return 2;}
    } else if(argc!=1) {std::cerr<<"Usage: hookshot_vr_scene [--seconds 1..90]\n";return 2;}
    try {
        Scene scene;scene.init();scene.run(seconds);scene.report();
        return scene.rendered && scene.head_samples?0:3;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
