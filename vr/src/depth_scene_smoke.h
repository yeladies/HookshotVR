// SPDX-License-Identifier: GPL-3.0-or-later
// Included after the diagnostic Vulkan loader; no OpenXR instance/session is created.
int depth_scene_smoke(const std::string& mesh_path,const std::string& prefix,bool streaming=false,int seconds=0){
    Vulkan vk;vk.load();
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="HookshotVR depth mesh offscreen check";app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;
    auto create=reinterpret_cast<PFN_vkCreateInstance>(vk.get(VK_NULL_HANDLE,"vkCreateInstance"));if(!create)throw std::runtime_error("No Vulkan instance function");vk_check(create(&ci,nullptr,&vk.instance),"offscreen instance");
    vk.destroy_instance=vk.function<PFN_vkDestroyInstance>("vkDestroyInstance");
    auto enumerate=vk.function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");uint32_t count=0;vk_check(enumerate(vk.instance,&count,nullptr),"count GPUs");if(!count)throw std::runtime_error("No Vulkan GPU");
    std::vector<VkPhysicalDevice> devices(count);vk_check(enumerate(vk.instance,&count,devices.data()),"GPUs");vk.physical=devices[0];
    auto families_fn=vk.function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");families_fn(vk.physical,&count,nullptr);std::vector<VkQueueFamilyProperties> families(count);families_fn(vk.physical,&count,families.data());
    bool found=false;for(uint32_t i=0;i<count;++i)if(families[i].queueCount&&(families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){vk.queue_family=i;found=true;break;}if(!found)throw std::runtime_error("No graphics queue");
    const float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=vk.queue_family;queue.queueCount=1;queue.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};di.queueCreateInfoCount=1;di.pQueueCreateInfos=&queue;vk_check(vk.function<PFN_vkCreateDevice>("vkCreateDevice")(vk.physical,&di,nullptr,&vk.device),"offscreen device");vk.device_functions();vk.GetDeviceQueue(vk.device,vk.queue_family,0,&vk.queue);
    auto function=[&](const char* name){auto p=vk.get_device(vk.device,name);if(!p)throw std::runtime_error(name);return p;};
#define F(type,name) auto name=reinterpret_cast<PFN_vk##type>(function("vk" #type))
    F(CreateImage,create_image);F(GetImageMemoryRequirements,image_req);F(AllocateMemory,allocate);F(BindImageMemory,bind_image);F(FreeMemory,free_memory);F(DestroyImage,destroy_image);
    F(CreateBuffer,create_buffer);F(GetBufferMemoryRequirements,buffer_req);F(BindBufferMemory,bind_buffer);F(DestroyBuffer,destroy_buffer);F(MapMemory,map);F(UnmapMemory,unmap);F(CmdPipelineBarrier,barrier);F(CmdCopyImageToBuffer,copy);
#undef F
    VkPhysicalDeviceMemoryProperties memory_props{};auto memory_properties=vk.function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties");memory_properties(vk.physical,&memory_props);
    auto type=[&](uint32_t bits,VkMemoryPropertyFlags flags){for(uint32_t i=0;i<memory_props.memoryTypeCount;++i)if((bits&(1u<<i))&&(memory_props.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("No offscreen memory type");};
    const uint32_t width=800,height=600;const VkDeviceSize bytes=uint64_t(width)*height*4;
    VkAttachmentDescription attachments[2]{};attachments[0].format=VK_FORMAT_R8G8B8A8_SRGB;attachments[1].format=VK_FORMAT_D32_SFLOAT;
    for(auto& a:attachments){a.samples=VK_SAMPLE_COUNT_1_BIT;a.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;a.storeOp=VK_ATTACHMENT_STORE_OP_STORE;a.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;a.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;a.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;}
    attachments[0].finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkAttachmentReference color_ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth_ref{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color_ref;subpass.pDepthStencilAttachment=&depth_ref;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};rp.attachmentCount=2;rp.pAttachments=attachments;rp.subpassCount=1;rp.pSubpasses=&subpass;VkRenderPass pass=VK_NULL_HANDLE;vk_check(vk.CreateRenderPass(vk.device,&rp,nullptr,&pass),"offscreen pass");
    hookshot::vr::DepthSceneRenderer renderer;if(streaming)renderer.load_stream(mesh_path);else renderer.load(mesh_path);renderer.initialize(vk.device,vk.physical,vk.get_device,memory_properties,pass);
    VkImage image=VK_NULL_HANDLE;VkDeviceMemory image_memory=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkFramebuffer framebuffer=VK_NULL_HANDLE;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=VK_FORMAT_R8G8B8A8_SRGB;ii.extent={width,height,1};ii.mipLevels=ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    vk_check(create_image(vk.device,&ii,nullptr,&image),"offscreen image");VkMemoryRequirements req{};image_req(vk.device,image,&req);VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=req.size;allocation.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);vk_check(allocate(vk.device,&allocation,nullptr,&image_memory),"offscreen color memory");vk_check(bind_image(vk.device,image,image_memory,0),"bind offscreen image");
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ii.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vk_check(vk.CreateImageView(vk.device,&vi,nullptr,&view),"offscreen view");
    VkImageView views[2]={view,renderer.depth(0,width,height)};VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=2;fi.pAttachments=views;fi.width=width;fi.height=height;fi.layers=1;vk_check(vk.CreateFramebuffer(vk.device,&fi,nullptr,&framebuffer),"offscreen framebuffer");
    VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory buffer_memory=VK_NULL_HANDLE;void* mapped=nullptr;VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=bytes;bi.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;bi.sharingMode=VK_SHARING_MODE_EXCLUSIVE;vk_check(create_buffer(vk.device,&bi,nullptr,&buffer),"offscreen readback");buffer_req(vk.device,buffer,&req);allocation.allocationSize=req.size;try{allocation.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT);}catch(...){allocation.memoryTypeIndex=type(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);}vk_check(allocate(vk.device,&allocation,nullptr,&buffer_memory),"readback memory");vk_check(bind_buffer(vk.device,buffer,buffer_memory,0),"bind readback");vk_check(map(vk.device,buffer_memory,0,bytes,0,&mapped),"map readback");
    VkCommandPool pool=VK_NULL_HANDLE;VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;pi.queueFamilyIndex=vk.queue_family;vk_check(vk.CreateCommandPool(vk.device,&pi,nullptr,&pool),"offscreen command pool");
    VkCommandBuffer command=VK_NULL_HANDLE;VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;vk_check(vk.AllocateCommandBuffers(vk.device,&ca,&command),"offscreen command");VkFence fence=VK_NULL_HANDLE;VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};vk_check(vk.CreateFence(vk.device,&fc,nullptr,&fence),"offscreen fence");
    std::vector<uint8_t> left,previous;uint64_t lit_pixels=0,different=0,temporal_different=0,pairs=0,render_us=0,readback_us=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    do{
    if(pairs){std::this_thread::sleep_for(std::chrono::milliseconds(20));if(!renderer.refresh_stream())continue;}
    lit_pixels=0;different=0;
    for(unsigned eye=0;eye<2;++eye){
        const auto render_start=std::chrono::steady_clock::now();
        vk_check(vk.ResetCommandBuffer(command,0),"reset offscreen command");VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};cb.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;vk_check(vk.BeginCommandBuffer(command,&cb),"begin offscreen command");
        VkClearValue clear[2]{};clear[0].color={{0,0,0,1}};clear[1].depthStencil={1,0};VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=pass;begin.framebuffer=framebuffer;begin.renderArea={{0,0},{width,height}};begin.clearValueCount=2;begin.pClearValues=clear;
        vk.CmdBeginRenderPass(command,&begin,VK_SUBPASS_CONTENTS_INLINE);XrPosef anchor{},pose{};anchor.orientation.w=pose.orientation.w=1;pose.position.x=eye?.032f:-.032f;
        renderer.draw(command,width,height,hookshot::vr::depth_scene_mvp(pose,anchor,{-.65f,.65f,.65f,-.65f}));vk.CmdEndRenderPass(command);
        VkMemoryBarrier m{VK_STRUCTURE_TYPE_MEMORY_BARRIER};m.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;m.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier(command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&m,0,nullptr,0,nullptr);
        VkBufferImageCopy region{};region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};region.imageExtent={width,height,1};copy(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffer,1,&region);
        m.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;m.dstAccessMask=VK_ACCESS_HOST_READ_BIT;barrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&m,0,nullptr,0,nullptr);vk_check(vk.EndCommandBuffer(command),"end offscreen command");
        vk_check(vk.ResetFences(vk.device,1,&fence),"reset offscreen fence");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;vk_check(vk.QueueSubmit(vk.queue,1,&submit,fence),"offscreen submit");vk_check(vk.WaitForFences(vk.device,1,&fence,VK_TRUE,5000000000ull),"offscreen completion");
        const auto read_start=std::chrono::steady_clock::now();render_us+=std::chrono::duration_cast<std::chrono::microseconds>(read_start-render_start).count();
        std::vector<uint8_t> cached(size_t(bytes),0);std::memcpy(cached.data(),mapped,size_t(bytes));const auto* pixels=cached.data();std::vector<uint8_t> rgb(size_t(width)*height*3);
        std::ofstream file(std::filesystem::u8path(prefix+(eye?"-right.ppm":"-left.ppm")),std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
        for(uint64_t i=0;i<bytes;i+=4){std::memcpy(rgb.data()+i/4*3,pixels+i,3);if(pixels[i]||pixels[i+1]||pixels[i+2])++lit_pixels;if(eye&&std::memcmp(pixels+i,left.data()+i,3))++different;}
        file.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));file.close();
        readback_us+=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-read_start).count();
        if(!eye){if(!previous.empty())for(uint64_t i=0;i<bytes;i+=4)temporal_different+=std::memcmp(pixels+i,previous.data()+i,3)!=0;left.assign(pixels,pixels+bytes);previous=left;}
    }
    ++pairs;
    }while(streaming&&seconds>0&&std::chrono::steady_clock::now()<deadline);
    vk_check(vk.DeviceWaitIdle(vk.device),"offscreen drain");vk.DestroyFence(vk.device,fence,nullptr);vk.DestroyCommandPool(vk.device,pool,nullptr);vk.DestroyFramebuffer(vk.device,framebuffer,nullptr);vk.DestroyImageView(vk.device,view,nullptr);destroy_image(vk.device,image,nullptr);free_memory(vk.device,image_memory,nullptr);unmap(vk.device,buffer_memory);destroy_buffer(vk.device,buffer,nullptr);free_memory(vk.device,buffer_memory,nullptr);renderer.close();vk.DestroyRenderPass(vk.device,pass,nullptr);
    std::cout<<"{\"offscreen\":true,\"XR_session\":false,\"lit_pixels\":"<<lit_pixels<<",\"stereo_different_pixels\":"<<different<<",\"rendered_pairs\":"<<pairs<<",\"capture_updates\":"<<renderer.updates()<<",\"source_color_changed_frames\":"<<renderer.color_changes()<<",\"source_depth_changed_frames\":"<<renderer.depth_changes()<<",\"temporal_different_pixels\":"<<temporal_different<<",\"mean_render_pair_us\":"<<(pairs?render_us/pairs:0)<<",\"mean_readback_pair_us\":"<<(pairs?readback_us/pairs:0)<<"}\n";
    return lit_pixels>1000&&different>100&&(!streaming||seconds==0||(pairs>2&&temporal_different>100))?0:3;
}
