// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit opt-in native GPU smoke. No game, XR runtime, window or display changes.
#define VK_NO_PROTOTYPES
#include "eye_vulkan_resources.h"
#include "eye_uniform_copy.h"
#include "camera_math.h"
#include "eye_address_copy_spv.h"
#include "../../gpu/vk/xr_eye_matrix_patch_spv.h"
#include "eye_page_patches.h"
#include "eye_buffer_descriptors.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif
using namespace hookshot::vr;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void checked(VkResult r,const char* why){if(r!=VK_SUCCESS)throw std::runtime_error(std::string(why)+": "+std::to_string(r));}
struct AddressBuffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;VkDeviceAddress address=0;uint64_t bytes=0;};
struct Context {
    PFN_vkGetInstanceProcAddr get=nullptr;PFN_vkGetDeviceProcAddr gd=nullptr;
    VkInstance instance=VK_NULL_HANDLE;VkDevice device=VK_NULL_HANDLE;VkQueue queue=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;VkBuffer staging=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;
    std::array<AddressBuffer,4> auxiliary{};uint64_t auxiliary_bytes=0;
    VkDescriptorPool descriptor_pool=VK_NULL_HANDLE;VkDescriptorSetLayout descriptor_layout=VK_NULL_HANDLE;
    std::array<VkDescriptorSet,2> descriptor_sets{};
    VkShaderModule matrix_shader=VK_NULL_HANDLE;VkPipeline matrix_pipeline=VK_NULL_HANDLE;
    VkShaderModule shader=VK_NULL_HANDLE;VkPipelineLayout layout=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    bool uncertain=false;
    template<class T>T global(const char* name){auto fn=get(instance,name);require(fn!=nullptr,name);return reinterpret_cast<T>(fn);}
    template<class T>T fn(const char* name){auto p=gd(device,name);require(p!=nullptr,name);return reinterpret_cast<T>(p);}
    ~Context(){
        // On uncertain execution retain everything for process exit, including device/loader.
        if(uncertain)return;
        if(device){
            for(auto& buffer:auxiliary){
                if(buffer.buffer)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,buffer.buffer,nullptr);
                if(buffer.memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,buffer.memory,nullptr);
            }
            if(descriptor_pool)fn<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(device,descriptor_pool,nullptr);
            if(matrix_pipeline)fn<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device,matrix_pipeline,nullptr);
            if(matrix_shader)fn<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device,matrix_shader,nullptr);
            if(pipeline)fn<PFN_vkDestroyPipeline>("vkDestroyPipeline")(device,pipeline,nullptr);
            if(layout)fn<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(device,layout,nullptr);
            if(descriptor_layout)fn<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(device,descriptor_layout,nullptr);
            if(shader)fn<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(device,shader,nullptr);
            if(pool)fn<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(device,pool,nullptr);
            if(staging)fn<PFN_vkDestroyBuffer>("vkDestroyBuffer")(device,staging,nullptr);
            if(memory)fn<PFN_vkFreeMemory>("vkFreeMemory")(device,memory,nullptr);
            fn<PFN_vkDestroyDevice>("vkDestroyDevice")(device,nullptr);
        }
        if(instance)global<PFN_vkDestroyInstance>("vkDestroyInstance")(instance,nullptr);
    }
};
void address_buffer(Context& c,const EyeResourceApi& api,const VkPhysicalDeviceMemoryProperties& memory,uint64_t bytes,AddressBuffer& out){
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;
    bi.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    checked(api.create_buffer(c.device,&bi,nullptr,&out.buffer),"create auxiliary address buffer");
    VkMemoryRequirements req{};api.buffer_requirements(c.device,out.buffer,&req);
    require(req.size&&req.size<=96ull*1024*1024-c.auxiliary_bytes,"auxiliary actual-allocation budget");
    uint32_t type=UINT32_MAX;
    for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)){type=i;break;}
    require(type!=UINT32_MAX,"auxiliary device-local memory required");
    VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&flags;ai.allocationSize=req.size;ai.memoryTypeIndex=type;
    checked(api.allocate(c.device,&ai,nullptr,&out.memory),"allocate auxiliary address storage");c.auxiliary_bytes+=req.size;
    checked(api.bind_buffer(c.device,out.buffer,out.memory,0),"bind auxiliary address storage");
    VkBufferDeviceAddressInfo info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};info.buffer=out.buffer;
    out.address=api.buffer_address(c.device,&info);out.bytes=bytes;
    require(out.address&&out.address%16==0&&out.address<=UINT64_MAX-bytes,"valid auxiliary device address");
}
int main(int argc,char** argv){
    const bool reject_ubo=argc==3&&!std::strcmp(argv[2],"--expect-ubo-rejection");
    const bool reject_ssbo=argc==3&&!std::strcmp(argv[2],"--expect-ssbo-rejection");
    const bool reject_matrix_shift=argc==3&&!std::strcmp(argv[2],"--matrix-patch-reject-shift");
    const bool reject_matrix_view=argc==3&&!std::strcmp(argv[2],"--matrix-patch-reject-view");
    const bool matrix_patch=(argc==3&&!std::strcmp(argv[2],"--matrix-patch"))||reject_matrix_shift||reject_matrix_view;
    if(argc<2||argc>3||std::strcmp(argv[1],"--run")||(argc==3&&!reject_ubo&&!reject_ssbo&&!matrix_patch)){
        std::cerr<<"Explicit native GPU opt-in: --run [--expect-ubo-rejection|--expect-ssbo-rejection|--matrix-patch|--matrix-patch-reject-shift|--matrix-patch-reject-view]\n";return 2;
    }
    const bool expect_rejection=reject_ubo||reject_ssbo;
    try {
        Context c;
#ifdef _WIN32
        auto library=LoadLibraryExW(L"vulkan-1.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library,"vkGetInstanceProcAddr"));
#else
        auto library=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
        if(library)c.get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library,"vkGetInstanceProcAddr"));
#endif
        require(c.get,"System Vulkan loader unavailable");
        VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="HookshotVR private-eye resource smoke";app.apiVersion=VK_API_VERSION_1_2;
        VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
        checked(c.global<PFN_vkCreateInstance>("vkCreateInstance")(&ici,nullptr,&c.instance),"create instance");
        auto enumerate=c.global<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");uint32_t count=0;checked(enumerate(c.instance,&count,nullptr),"enumerate count");require(count>0&&count<=32,"bounded physical devices");
        std::vector<VkPhysicalDevice> devices(count);checked(enumerate(c.instance,&count,devices.data()),"enumerate GPU");
        VkPhysicalDevice physical=VK_NULL_HANDLE;uint32_t family=0;
        for(auto candidate:devices){uint32_t n=0;auto queues=c.global<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");queues(candidate,&n,nullptr);require(n<=256,"bounded queue families");std::vector<VkQueueFamilyProperties> q(n);queues(candidate,&n,q.data());
            for(uint32_t i=0;i<n;++i)if(q[i].queueCount&&((q[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))){physical=candidate;family=i;break;}
            if(physical)break;
        }
        require(physical,"graphics queue unavailable");VkPhysicalDeviceProperties props{};c.global<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(physical,&props);
        VkPhysicalDeviceMemoryProperties memory{};c.global<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(physical,&memory);
        require(props.apiVersion>=VK_API_VERSION_1_2,"Vulkan 1.2 required for native device-address smoke");
        VkPhysicalDeviceBufferDeviceAddressFeatures address_features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&address_features;
        c.global<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(physical,&features);
        require(address_features.bufferDeviceAddress,"buffer device address unavailable");
        address_features.bufferDeviceAddressCaptureReplay=VK_FALSE;address_features.bufferDeviceAddressMultiDevice=VK_FALSE;
        float priority=1;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;
        VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.pNext=&address_features;di.queueCreateInfoCount=1;di.pQueueCreateInfos=&qi;
        checked(c.global<PFN_vkCreateDevice>("vkCreateDevice")(physical,&di,nullptr,&c.device),"create device");
        c.gd=c.global<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");c.fn<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(c.device,family,0,&c.queue);
        EyeResourceApi api;
#define LOAD(field,name) api.field=c.fn<PFN_vk##name>("vk" #name)
        LOAD(create_buffer,CreateBuffer);LOAD(destroy_buffer,DestroyBuffer);LOAD(buffer_requirements,GetBufferMemoryRequirements);LOAD(bind_buffer,BindBufferMemory);
        LOAD(create_image,CreateImage);LOAD(destroy_image,DestroyImage);LOAD(image_requirements,GetImageMemoryRequirements);LOAD(bind_image,BindImageMemory);
        LOAD(allocate,AllocateMemory);LOAD(free,FreeMemory);LOAD(create_fence,CreateFence);LOAD(destroy_fence,DestroyFence);LOAD(submit,QueueSubmit);LOAD(wait,WaitForFences);LOAD(reset,ResetFences);
#undef LOAD
        api.buffer_address=c.fn<PFN_vkGetBufferDeviceAddress>("vkGetBufferDeviceAddress");
        // Entirely authored camera payload: no game data or measured world calibration.
        std::vector<std::byte> original(4096);
        for(size_t i=0;i<original.size();++i)original[i]=std::byte((i*37)%256);
        float base[0xe0/4]{};for(int i=0;i<3;++i)base[4*i+i]=1;
        ProjectionMatrix projection{};require(eye_projection({-.7,.8,.9,-.6},.05,1000,ClipDepth::MinusOneToOne,DepthOrder::Forward,false,projection),"authored projection");
        for(int i=0;i<16;++i)base[0x70/4+i]=float(projection[i]);
        if(matrix_patch){CameraView view;TrackingOrigin origin{};origin.yaw_radians=.3;
            XrPosef pose{{float(std::sin(.1)),0,0,float(std::cos(.1))},{1,2,3}};
            require(tracked_camera_view(origin,pose,3,view),"authored pitched source camera");
            for(int r=0;r<3;++r)for(int col=0;col<4;++col)base[4*r+col]=float(view.rows[r][col]);}
        for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)
            base[12+4*r+col]+=base[28+4*r+k]*(k==3?(col==3?1.f:0.f):base[4*k+col]);
        uniform_detail::inverse(base,base+0xb0/4);std::memcpy(original.data(),base,sizeof base);
        if(reject_matrix_view){float invalid=2;std::memcpy(original.data(),&invalid,4);}
        const auto source_before=original;std::array<std::vector<std::byte>,2> payloads;
        for(unsigned eye=0;eye<2;++eye){
            if(reject_matrix_shift||reject_matrix_view){payloads[eye]=original;continue;}
            EyeMatrices matrices{};TrackingOrigin origin{};origin.yaw_radians=.3;
            XrPosef pose{{0,0,float(std::sin(.1)),float(std::cos(.1))},{eye?.032f:-.032f,1,2}};
            require(tracked_camera_view(origin,pose,XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT,matrices.view),"authored tracked eye");
            if(matrix_patch){
                for(int r=0;r<3;++r)for(int col=0;col<4;++col)matrices.view.rows[r][col]=base[4*r+col];
                matrices.view.rows[0][3]=float(base[3]+(eye?.01f:-.01f));
                float view[12],inverse[12];for(int i=0;i<12;++i)view[i]=float(matrices.view.rows[i/4][i%4]);
                uniform_detail::inverse(view,inverse);for(int r=0;r<3;++r)matrices.view.eye[r]=inverse[4*r+3];
            }
            matrices.projection=projection;
            for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)
                matrices.world_to_clip[4*r+col]+=projection[4*r+k]*(k==3?(col==3?1:0):matrices.view.rows[k][col]);
            require(prepare_eye_uniform_copy(original.data(),original.size(),matrices,ClipDepth::MinusOneToOne,payloads[eye]),"prepare complete eye payload");
        }
        require(original==source_before&&((reject_matrix_shift||reject_matrix_view)||payloads[0]!=payloads[1]),"source preserved and eye payloads distinct");
        EyeResourcePair pair;require(pair.create(c.device,api,memory,32,32,VK_FORMAT_R8G8B8A8_UNORM,4096,8*1024*1024,true),"create bounded independent eyes");
        require(pair.eye(0)->address&&pair.eye(1)->address&&pair.eye(0)->address!=pair.eye(1)->address,"independent buffer device addresses");
        address_buffer(c,api,memory,8192,c.auxiliary[0]);
        for(unsigned index=1;index<4;++index)address_buffer(c,api,memory,kEyeGuestTableBytes,c.auxiliary[index]);
        constexpr uint64_t guest_a=0x1000000000ull,guest_b=0x7f00000000ull;
        const EyePageStamp stamp{1,1,1,1};EyeGuestPagePlan plan;
        require(EyeGuestPagePlan::build({{1,0,4096}},{},{{guest_a,1,0},{guest_b,1,0},{guest_a+4096,1,4096}},
            {{1,8192,c.auxiliary[0].address}},stamp,stamp,256,4096,plan),"build authored full-page plan");
        std::vector<uint64_t> original_root(kEyeGuestPageCount);original_root[0]=original_root[*eye_guest_page_index(guest_b)]=c.auxiliary[0].address;
        original_root[1]=c.auxiliary[0].address+4096;
        const auto root_before=original_root;EyePagePatchSet patches;
        require(prepare_eye_page_patches(plan,stamp,original_root.data(),original_root.size(),
            {{pair.eye(0)->address,4096},{pair.eye(1)->address,4096}},patches)&&original_root==root_before,"encode patches preserving original root");
        const auto descriptor_usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        EyeBufferDescriptors descriptor_plan;
        const EyeDescriptorLimits descriptor_limits{props.limits.minUniformBufferOffsetAlignment,props.limits.minStorageBufferOffsetAlignment,
            props.limits.maxUniformBufferRange,props.limits.maxStorageBufferRange};
        require(prepare_eye_buffer_descriptors(plan.buffers(),{{1,c.auxiliary[0].buffer,8192,descriptor_usage}},
            {{pair.eye(0)->buffer,4096,descriptor_usage},{pair.eye(1)->buffer,4096,descriptor_usage}},
            {{1,0,4096,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,false,false},{1,0,4096,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,false,false}},
            stamp,stamp,descriptor_limits,descriptor_plan),"prepare coherent private UBO/SSBO routes");
        const std::array<VkDescriptorSetLayoutBinding,2> descriptor_bindings{{
            {0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
            {1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
        VkDescriptorSetLayoutCreateInfo descriptor_layout_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptor_layout_info.bindingCount=2;descriptor_layout_info.pBindings=descriptor_bindings.data();
        checked(c.fn<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(c.device,&descriptor_layout_info,nullptr,&c.descriptor_layout),"create private descriptor layout");
        const std::array<VkDescriptorPoolSize,2> pool_sizes{{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2}}};
        VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool_info.maxSets=2;pool_info.poolSizeCount=2;pool_info.pPoolSizes=pool_sizes.data();
        checked(c.fn<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(c.device,&pool_info,nullptr,&c.descriptor_pool),"create private descriptor pool");
        const std::array<VkDescriptorSetLayout,2> layouts{c.descriptor_layout,c.descriptor_layout};
        VkDescriptorSetAllocateInfo set_info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};set_info.descriptorPool=c.descriptor_pool;set_info.descriptorSetCount=2;set_info.pSetLayouts=layouts.data();
        checked(c.fn<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(c.device,&set_info,c.descriptor_sets.data()),"allocate per-eye descriptor sets");
        std::array<VkWriteDescriptorSet,4> writes{};
        for(unsigned eye=0;eye<2;++eye)for(unsigned binding=0;binding<2;++binding){auto& write=writes[eye*2+binding];
            write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;write.dstSet=c.descriptor_sets[eye];write.dstBinding=binding;write.descriptorCount=1;
            write.descriptorType=binding?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            const bool swap=(binding==0&&reject_ssbo)||(binding==1&&reject_ubo);
            write.pBufferInfo=&descriptor_plan.eyes[swap?(eye^1):eye][binding];}
        c.fn<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(c.device,4,writes.data(),0,nullptr);
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=2*(32*32*4+4096)+3*4096;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
        checked(api.create_buffer(c.device,&bi,nullptr,&c.staging),"create readback");VkMemoryRequirements req{};api.buffer_requirements(c.device,c.staging,&req);require(req.size<=1024*1024,"bounded staging allocation");
        uint32_t type=UINT32_MAX;bool coherent=false;
        for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((req.memoryTypeBits&(1u<<i))&&(memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)){type=i;coherent=memory.memoryTypes[i].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;break;}
        require(type!=UINT32_MAX,"host-visible readback memory");VkMemoryAllocateFlagsInfo address_flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO};address_flags.flags=VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.pNext=&address_flags;ai.allocationSize=req.size;ai.memoryTypeIndex=type;checked(api.allocate(c.device,&ai,nullptr,&c.memory),"allocate readback");checked(api.bind_buffer(c.device,c.staging,c.memory,0),"bind readback");
        VkBufferDeviceAddressInfo staging_info{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};staging_info.buffer=c.staging;
        const auto staging_address=api.buffer_address(c.device,&staging_info);
        require(staging_address&&staging_address%16==0&&staging_address<=UINT64_MAX-bi.size,"valid staging device address");
        VkShaderModuleCreateInfo shader_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};shader_info.codeSize=sizeof kEyeAddressCopySpv;shader_info.pCode=kEyeAddressCopySpv;
        checked(c.fn<PFN_vkCreateShaderModule>("vkCreateShaderModule")(c.device,&shader_info,nullptr,&c.shader),"create authored address shader");
        VkPushConstantRange range{VK_SHADER_STAGE_COMPUTE_BIT,0,24};VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};layout_info.setLayoutCount=1;layout_info.pSetLayouts=&c.descriptor_layout;layout_info.pushConstantRangeCount=1;layout_info.pPushConstantRanges=&range;
        checked(c.fn<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(c.device,&layout_info,nullptr,&c.layout),"create address pipeline layout");
        VkComputePipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pipeline_info.layout=c.layout;
        pipeline_info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pipeline_info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pipeline_info.stage.module=c.shader;pipeline_info.stage.pName="main";
        checked(c.fn<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(c.device,VK_NULL_HANDLE,1,&pipeline_info,nullptr,&c.pipeline),"create address compute pipeline");
        if(matrix_patch){
            shader_info.codeSize=sizeof kEyeMatrixPatchSpv;shader_info.pCode=kEyeMatrixPatchSpv;
            checked(c.fn<PFN_vkCreateShaderModule>("vkCreateShaderModule")(c.device,&shader_info,nullptr,&c.matrix_shader),"create matrix patch shader");
            pipeline_info.stage.module=c.matrix_shader;
            checked(c.fn<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(c.device,VK_NULL_HANDLE,1,&pipeline_info,nullptr,&c.matrix_pipeline),"create matrix patch pipeline");
        }
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;checked(c.fn<PFN_vkCreateCommandPool>("vkCreateCommandPool")(c.device,&pi,nullptr,&c.pool),"command pool");
        VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=c.pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;VkCommandBuffer command=VK_NULL_HANDLE;
        checked(c.fn<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(c.device,&ca,&command),"command buffer");for(unsigned round=0;round<2;++round){
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;checked(c.fn<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(command,&begin),"begin");
        auto barrier=c.fn<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier");
        if(round==0){
            std::vector<std::byte> source_pages=original;source_pages.insert(source_pages.end(),original.begin(),original.end());
            c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[0].buffer,0,8192,source_pages.data());
            c.fn<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(command,c.auxiliary[3].buffer,0,kEyeGuestTableBytes,0);
            VkMemoryBarrier table_write{VK_STRUCTURE_TYPE_MEMORY_BARRIER};table_write.srcAccessMask=table_write.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&table_write,0,nullptr,0,nullptr);
            for(auto guest:{guest_a,guest_a+4096,guest_b}){
                auto index=*eye_guest_page_index(guest);const auto value=original_root[index];
                c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[3].buffer,uint64_t(index)*8,8,&value);
            }
            table_write.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
            barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&table_write,0,nullptr,0,nullptr);
            VkBufferCopy root_copy{0,0,kEyeGuestTableBytes};
            for(unsigned eye=0;eye<2;++eye)c.fn<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(command,c.auxiliary[3].buffer,c.auxiliary[eye+1].buffer,1,&root_copy);
            table_write.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&table_write,0,nullptr,0,nullptr);
            for(unsigned eye=0;eye<2;++eye)for(auto patch:patches.eyes[eye])
                c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,c.auxiliary[eye+1].buffer,uint64_t(patch.index)*8,8,&patch.address);
        }
        auto read_page=[&](VkDeviceAddress root,uint32_t page,VkDeviceAddress destination,bool check_descriptors=false){
            struct Parameters {VkDeviceAddress root,destination;uint32_t page,padding;};
            const Parameters addresses{root,destination,page,check_descriptors?1u:0u};static_assert(sizeof addresses==24);
            c.fn<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(command,VK_PIPELINE_BIND_POINT_COMPUTE,c.pipeline);
            c.fn<PFN_vkCmdPushConstants>("vkCmdPushConstants")(command,c.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof addresses,&addresses);
            c.fn<PFN_vkCmdDispatch>("vkCmdDispatch")(command,16,1,1);
        };
        for(unsigned eye=0;eye<2;++eye){
            const auto& e=*pair.eye(eye);VkImageMemoryBarrier ib{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};ib.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ib.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;ib.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;ib.srcQueueFamilyIndex=ib.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;ib.image=e.image;ib.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
            barrier(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&ib);
            VkClearColorValue color{};color.float32[(eye+round)%2]=1;color.float32[3]=1;
            c.fn<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(command,e.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&ib.subresourceRange);
            if(matrix_patch){
                VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
                barrier(command,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&ready,0,nullptr,0,nullptr);
                struct MatrixParameters {VkDeviceAddress source,destination;uint32_t bytes;float shift;};static_assert(sizeof(MatrixParameters)==24);
                const MatrixParameters parameters{c.auxiliary[0].address,e.address,4096,reject_matrix_shift?.1f:(eye?.01f:-.01f)};
                c.fn<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(command,VK_PIPELINE_BIND_POINT_COMPUTE,c.matrix_pipeline);
                c.fn<PFN_vkCmdPushConstants>("vkCmdPushConstants")(command,c.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof parameters,&parameters);
                c.fn<PFN_vkCmdDispatch>("vkCmdDispatch")(command,1,1,1);
            }else c.fn<PFN_vkCmdUpdateBuffer>("vkCmdUpdateBuffer")(command,e.buffer,0,4096,payloads[eye].data());
            ib.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ib.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;ib.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;ib.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};mb.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_UNIFORM_READ_BIT;
            barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&mb,0,nullptr,1,&ib);
            VkBufferImageCopy copy{};copy.bufferOffset=eye*8192;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={32,32,1};
            c.fn<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(command,e.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,c.staging,1,&copy);
            c.fn<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(command,VK_PIPELINE_BIND_POINT_COMPUTE,c.layout,0,1,&c.descriptor_sets[eye],0,nullptr);
            read_page(c.auxiliary[eye+1].address,*eye_guest_page_index((round+eye)%2?guest_b:guest_a),staging_address+eye*8192+4096,true);
            read_page(c.auxiliary[eye+1].address,1,staging_address+16384+eye*4096);
        }
        read_page(c.auxiliary[3].address,0,staging_address+24576);
        VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
        barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
        checked(c.fn<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(command),"end");
        const auto submitted=pair.submit(c.queue,command);c.uncertain=pair.pending();checked(submitted,"submit eyes");
        require(pair.poll(5000000000ull),"GPU fence not complete within five seconds; resources retained");c.uncertain=false;
        void* mapped=nullptr;checked(c.fn<PFN_vkMapMemory>("vkMapMemory")(c.device,c.memory,0,VK_WHOLE_SIZE,0,&mapped),"map readback");
        if(!coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=c.memory;range.size=VK_WHOLE_SIZE;checked(c.fn<PFN_vkInvalidateMappedMemoryRanges>("vkInvalidateMappedMemoryRanges")(c.device,1,&range),"invalidate readback");}
        bool pixels_correct=true,original_correct=true;std::array<unsigned,2> mismatches{};
        auto* bytes=static_cast<const unsigned char*>(mapped);
        for(unsigned eye=0;eye<2;++eye){for(unsigned pixel=0;pixel<1024;++pixel)for(unsigned channel=0;channel<4;++channel)
                pixels_correct&=bytes[eye*8192+pixel*4+channel]==((channel==(eye+round)%2||channel==3)?255:0);
            for(unsigned offset=0;offset<4096;++offset){
                if(matrix_patch&&!reject_matrix_shift&&!reject_matrix_view&&offset<0xe0&&(offset<0x70||offset>=0xb0))continue;
                mismatches[eye]+=bytes[eye*8192+4096+offset]!=std::to_integer<unsigned char>(payloads[eye][offset]);
            }
            if(matrix_patch&&!reject_matrix_shift&&!reject_matrix_view){
                float actual[56],expected[56];std::memcpy(actual,bytes+eye*8192+4096,sizeof actual);std::memcpy(expected,payloads[eye].data(),sizeof expected);
                for(int i=0;i<56;++i)mismatches[eye]+=!std::isfinite(actual[i])||std::abs(actual[i]-expected[i])>0.00003f*(1+std::abs(expected[i]));
                // Independently project a point five units ahead of the source camera.
                double world[4]={0,0,0,1};for(int r=0;r<3;++r)world[r]=base[44+4*r+3]-5*base[44+4*r+2];
                double bx=0,bw=0,x=0,w=0;for(int k=0;k<4;++k){bx+=base[12+k]*world[k];bw+=base[24+k]*world[k];x+=actual[12+k]*world[k];w+=actual[24+k]*world[k];}
                require(bw>0&&w>0&&(eye?x/w>bx/bw+.0001:x/w<bx/bw-.0001),"opposite projected-point displacements from the same source camera");
            }
        }
        for(unsigned offset=0;offset<4096;++offset){
            const auto expected=std::to_integer<unsigned char>(source_before[offset]);
            original_correct&=bytes[16384+offset]==expected&&bytes[20480+offset]==expected&&bytes[24576+offset]==expected;
        }
        c.fn<PFN_vkUnmapMemory>("vkUnmapMemory")(c.device,c.memory);
        require(pixels_correct&&original_correct,"image/shared/original root readback mismatch");
        for(auto count:mismatches)require(expect_rejection?count>0:count==0,"descriptor disagreement detection did not match the requested test mode");
        if(round==0){
            require(pair.rearm(),"rearm only after completion");
            checked(c.fn<PFN_vkResetCommandPool>("vkResetCommandPool")(c.device,c.pool,0),"reset completed command pool");
            if(!matrix_patch)for(auto& payload:payloads)payload[0x300]^=std::byte{7};
        }
        }
        if(matrix_patch)std::cout<<"gpu_matrix_patch=true; same_source_for_both_eyes=true; matrix_rejection_verified="<<((reject_matrix_shift||reject_matrix_view)?"true":"false")<<"; projected_point_displacement_verified="<<((!reject_matrix_shift&&!reject_matrix_view)?"true":"false")<<"; game_draw_transformed=false; ";
        std::cout<<"GPU="<<props.deviceName<<"; eye_allocation_bytes="<<pair.allocated_bytes()<<"; auxiliary_allocation_bytes="<<c.auxiliary_bytes<<"; independent_images=2; independent_buffers=2; disjoint_device_addresses=true; physical_address_shader_readback=true; private_ubo_routes=2; private_ssbo_routes=2; descriptor_root_agreement_verified="<<(!expect_rejection?"true":"false")<<"; expected_descriptor_rejection_verified="<<(expect_rejection?"true":"false")<<"; private_shader_words_checked=4096; shared_shader_words_checked=4096; original_root_shader_words_checked=2048; gpu_root_copy_verified=true; private_guest_roots=2; guest_table_bytes_per_eye=25165824; guest_window_aliases_checked=true; original_gpu_bytes_checked=8192; image_pixels_checked=4096; buffer_bytes_checked=16384; full_uniform_payloads_checked=4; submissions=2; resource_allocations_reused=true; original_payload_unchanged=true; fence_complete=true; XR=false; game=false\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
