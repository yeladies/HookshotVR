// Included inside this experiment's anonymous namespace, after State.
// A single head-relative dim panel, deliberately no game frame or camera patch.
int quad_test(State& s, PFN_xrGetInstanceProcAddr get, PFN_vkGetInstanceProcAddr vget,
              PFN_vkGetDeviceProcAddr dget, VkPhysicalDevice physical, XrSystemId system, uint32_t family,
              int seconds, uint32_t capture_pid) {
#define XR_FN(name) const auto name = xr_proc<PFN_##name>(get, s.instance, #name)
    XR_FN(xrEnumerateViewConfigurations); XR_FN(xrEnumerateEnvironmentBlendModes);
    XR_FN(xrCreateReferenceSpace); XR_FN(xrDestroySpace);
    XR_FN(xrEnumerateSwapchainFormats); XR_FN(xrCreateSwapchain); XR_FN(xrDestroySwapchain);
    XR_FN(xrEnumerateSwapchainImages); XR_FN(xrPollEvent); XR_FN(xrBeginSession); XR_FN(xrEndSession);
    XR_FN(xrRequestExitSession); XR_FN(xrWaitFrame); XR_FN(xrBeginFrame); XR_FN(xrEndFrame);
    XR_FN(xrAcquireSwapchainImage); XR_FN(xrWaitSwapchainImage); XR_FN(xrReleaseSwapchainImage);
#undef XR_FN
#define VK_FN(name) const auto name = reinterpret_cast<PFN_##name>(dget(s.vd, #name)); \
    if (!name) throw std::runtime_error("Missing " #name)
    VK_FN(vkCreateCommandPool); VK_FN(vkDestroyCommandPool); VK_FN(vkAllocateCommandBuffers);
    VK_FN(vkCreateFence); VK_FN(vkDestroyFence); VK_FN(vkResetFences); VK_FN(vkWaitForFences);
    VK_FN(vkResetCommandBuffer); VK_FN(vkBeginCommandBuffer); VK_FN(vkEndCommandBuffer);
    VK_FN(vkCmdPipelineBarrier); VK_FN(vkCmdClearColorImage); VK_FN(vkQueueSubmit); VK_FN(vkGetDeviceQueue);
    VK_FN(vkCreateBuffer); VK_FN(vkDestroyBuffer); VK_FN(vkGetBufferMemoryRequirements);
    VK_FN(vkAllocateMemory); VK_FN(vkFreeMemory); VK_FN(vkBindBufferMemory); VK_FN(vkMapMemory);
    VK_FN(vkUnmapMemory); VK_FN(vkCmdCopyBufferToImage);
#undef VK_FN
    const auto vkGetPhysicalDeviceMemoryProperties = vk_proc<PFN_vkGetPhysicalDeviceMemoryProperties>(vget, s.vi, "vkGetPhysicalDeviceMemoryProperties");
    struct Resources {
        State& s;
        PFN_xrDestroySpace destroy_space;
        PFN_xrDestroySwapchain destroy_swap;
        PFN_vkDestroyCommandPool destroy_pool;
        PFN_vkDestroyFence destroy_fence;
        PFN_vkDestroyBuffer destroy_buffer;
        PFN_vkFreeMemory free_memory;
        PFN_vkUnmapMemory unmap_memory;
        XrSpace space = XR_NULL_HANDLE;
        XrSwapchain swap = XR_NULL_HANDLE;
        VkCommandPool pool = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkBuffer upload = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        ~Resources() {
            if (fence) destroy_fence(s.vd, fence, nullptr);
            if (pool) destroy_pool(s.vd, pool, nullptr);
            if (mapped) unmap_memory(s.vd, memory);
            if (upload) destroy_buffer(s.vd, upload, nullptr);
            if (memory) free_memory(s.vd, memory, nullptr);
            if (swap) destroy_swap(swap);
            if (space) destroy_space(space);
        }
    } r{s, xrDestroySpace, xrDestroySwapchain, vkDestroyCommandPool, vkDestroyFence,
        vkDestroyBuffer, vkFreeMemory, vkUnmapMemory};
#ifdef HOOKSHOT_WINDOW_CAPTURE
    std::unique_ptr<WindowCapture> capture;
    if (capture_pid) capture = std::make_unique<WindowCapture>(capture_pid);
#else
    (void)capture_pid;
#endif
    uint32_t n = 0;
    xr_check(xrEnumerateViewConfigurations(s.instance, system, 0, &n, nullptr), "view config count");
    std::vector<XrViewConfigurationType> configs(n);
    xr_check(xrEnumerateViewConfigurations(s.instance, system, n, &n, configs.data()), "view configs");
    bool stereo = false;
    for (auto c : configs) stereo |= c == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    if (!stereo) throw std::runtime_error("PRIMARY_STEREO unavailable");
    xr_check(xrEnumerateEnvironmentBlendModes(s.instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &n, nullptr), "blend count");
    std::vector<XrEnvironmentBlendMode> blends(n);
    xr_check(xrEnumerateEnvironmentBlendModes(s.instance, system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, n, &n, blends.data()), "blend modes");
    bool opaque = false;
    for (auto b : blends) opaque |= b == XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    if (!opaque) throw std::runtime_error("Opaque blend mode unavailable");
    XrReferenceSpaceCreateInfo space{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    space.poseInReferenceSpace.orientation.w = 1;
    xr_check(xrCreateReferenceSpace(s.session, &space, &r.space), "xrCreateReferenceSpace");
    xr_check(xrEnumerateSwapchainFormats(s.session, 0, &n, nullptr), "format count");
    std::vector<int64_t> formats(n);
    xr_check(xrEnumerateSwapchainFormats(s.session, n, &n, formats.data()), "formats");
    int64_t format = VK_FORMAT_UNDEFINED;
    for (auto f : formats)
        if (f == VK_FORMAT_R8G8B8A8_UNORM || f == VK_FORMAT_B8G8R8A8_UNORM) { format = f; break; }
    if (format == VK_FORMAT_UNDEFINED) throw std::runtime_error("No supported UNORM panel format");
    if (capture_pid) {
        VkBufferCreateInfo b{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        b.size = 640 * 360 * 4; b.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        b.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vk_check(vkCreateBuffer(s.vd, &b, nullptr, &r.upload), "vkCreateBuffer(upload)");
        VkMemoryRequirements mem{};
        vkGetBufferMemoryRequirements(s.vd, r.upload, &mem);
        VkPhysicalDeviceMemoryProperties types{};
        vkGetPhysicalDeviceMemoryProperties(physical, &types);
        uint32_t type = 0;
        const auto flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        while (type < types.memoryTypeCount && (!(mem.memoryTypeBits & (1u << type)) ||
               (types.memoryTypes[type].propertyFlags & flags) != flags)) ++type;
        if (type == types.memoryTypeCount) throw std::runtime_error("No coherent upload memory");
        VkMemoryAllocateInfo a{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        a.allocationSize = mem.size; a.memoryTypeIndex = type;
        vk_check(vkAllocateMemory(s.vd, &a, nullptr, &r.memory), "vkAllocateMemory(upload)");
        vk_check(vkBindBufferMemory(s.vd, r.upload, r.memory, 0), "vkBindBufferMemory(upload)");
        vk_check(vkMapMemory(s.vd, r.memory, 0, b.size, 0, &r.mapped), "vkMapMemory(upload)");
        std::memset(r.mapped, 0, static_cast<size_t>(b.size));
    }
    XrSwapchainCreateInfo sci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    sci.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    sci.format = format;
    sci.sampleCount = 1;
    sci.width = 640; sci.height = 360;
    sci.faceCount = 1; sci.arraySize = 1; sci.mipCount = 1;
    xr_check(xrCreateSwapchain(s.session, &sci, &r.swap), "xrCreateSwapchain(transfer destination)");
    xr_check(xrEnumerateSwapchainImages(r.swap, 0, &n, nullptr), "swap image count");
    if (!n) throw std::runtime_error("No swapchain images");
    std::vector<XrSwapchainImageVulkan2KHR> images(n, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    xr_check(xrEnumerateSwapchainImages(r.swap, n, &n, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())), "swap images");
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = family;
    vk_check(vkCreateCommandPool(s.vd, &pool, nullptr, &r.pool), "vkCreateCommandPool");
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = r.pool; alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; alloc.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vk_check(vkAllocateCommandBuffers(s.vd, &alloc, &cmd), "vkAllocateCommandBuffers");
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    vk_check(vkCreateFence(s.vd, &fence, nullptr, &r.fence), "vkCreateFence");
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(s.vd, family, 0, &queue);
    bool running = false, exit_requested = false;
    uint64_t frames = 0, layers_sent = 0;
    uint64_t captured_frames = 0;
    uint64_t capture_changes = 0;
    [[maybe_unused]] uint64_t last_capture_hash = 0;
    using Clock = std::chrono::steady_clock;
    const auto deadline = Clock::now() + std::chrono::seconds(seconds);
    const auto hard_deadline = deadline + std::chrono::seconds(5);
    auto health_deadline = Clock::now() + std::chrono::seconds(1);
    while (Clock::now() < hard_deadline) {
        if (capture_pid && Clock::now() >= health_deadline) {
            check_capture_health(capture_pid);
            health_deadline = Clock::now() + std::chrono::seconds(1);
        }
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        for (;;) {
            const auto result = xrPollEvent(s.instance, &event);
            if (result == XR_EVENT_UNAVAILABLE) break;
            xr_check(result, "xrPollEvent");
            if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) throw std::runtime_error("XR instance loss pending");
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto& e = reinterpret_cast<const XrEventDataSessionStateChanged&>(event);
                std::printf("session_state=%d\n", static_cast<int>(e.state));
                if (e.state == XR_SESSION_STATE_READY && !running) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xr_check(xrBeginSession(s.session, &begin), "xrBeginSession");
                    running = true;
                } else if (e.state == XR_SESSION_STATE_STOPPING) {
                    if (running) xr_check(xrEndSession(s.session), "xrEndSession");
                    running = false;
                    std::printf("status=quad_finished frames=%llu layers_submitted=%llu captured_frames=%llu capture_changes=%llu visual_confirmation=required\n",
                                static_cast<unsigned long long>(frames), static_cast<unsigned long long>(layers_sent),
                                static_cast<unsigned long long>(captured_frames), static_cast<unsigned long long>(capture_changes));
                    return layers_sent && (!capture_pid || captured_frames) ? 0 : 3;
                } else if (e.state == XR_SESSION_STATE_EXITING || e.state == XR_SESSION_STATE_LOSS_PENDING)
                    throw std::runtime_error("XR session exiting or lost");
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        const bool escape = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (Clock::now() >= deadline || escape) {
            if (!running) break;
            if (!exit_requested) {
                std::printf("exit_requested=%s\n", escape ? "escape" : "deadline");
                xr_check(xrRequestExitSession(s.session), "xrRequestExitSession");
                exit_requested = true;
            }
        }
        if (!running) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState state{XR_TYPE_FRAME_STATE};
        xr_check(xrWaitFrame(s.session, &wait, &state), "xrWaitFrame");
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};
        xr_check(xrBeginFrame(s.session, &begin), "xrBeginFrame");
        XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* layer = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad);
        if (state.shouldRender && !exit_requested) {
            uint32_t index = 0;
            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            xr_check(xrAcquireSwapchainImage(r.swap, &acquire, &index), "xrAcquireSwapchainImage");
            if (index >= images.size()) throw std::runtime_error("Invalid XR image index");
            XrSwapchainImageWaitInfo image_wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            image_wait.timeout = 1000000000LL;
            const auto waited = xrWaitSwapchainImage(r.swap, &image_wait);
            if (waited != XR_SUCCESS) throw std::runtime_error("XR image wait failed/timed out: " + std::to_string(waited));
#ifdef HOOKSHOT_WINDOW_CAPTURE
            if (capture && capture->copy_pixels(static_cast<uint8_t*>(r.mapped), 640, 360, format == VK_FORMAT_B8G8R8A8_UNORM)) {
                uint64_t hash = 14695981039346656037ULL;
                const auto* bytes = static_cast<const uint8_t*>(r.mapped);
                for (size_t i = 0; i < 640 * 360 * 4; i += 13) hash = (hash ^ bytes[i]) * 1099511628211ULL;
                if (captured_frames && hash != last_capture_hash) ++capture_changes;
                last_capture_hash = hash;
                ++captured_frames;
            }
#endif
            vk_check(vkResetCommandBuffer(cmd, 0), "vkResetCommandBuffer");
            VkCommandBufferBeginInfo cb{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            cb.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vk_check(vkBeginCommandBuffer(cmd, &cb), "vkBeginCommandBuffer");
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = images[index].image;
            barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);
            if (captured_frames) {
                const VkBufferImageCopy copy{0, 0, 0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {640, 360, 1}};
                vkCmdCopyBufferToImage(cmd, r.upload, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            } else {
                const VkClearColorValue color{{0.025f, 0.08f, 0.14f, 1.0f}};
                vkCmdClearColorImage(cmd, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
            }
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
                                 0, nullptr, 0, nullptr, 1, &barrier);
            vk_check(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");
            vk_check(vkResetFences(s.vd, 1, &r.fence), "vkResetFences");
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
            vk_check(vkQueueSubmit(queue, 1, &submit, r.fence), "vkQueueSubmit");
            const auto done = vkWaitForFences(s.vd, 1, &r.fence, VK_TRUE, 5000000000ULL);
            if (done == VK_TIMEOUT) {
                // Do not destroy resources still in use after a stalled GPU. Let process teardown reclaim them.
                std::fprintf(stderr, "status=gpu_timeout; aborting isolated experiment\n");
                std::fflush(nullptr); std::_Exit(4);
            }
            vk_check(done, "vkWaitForFences");
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xr_check(xrReleaseSwapchainImage(r.swap, &release), "xrReleaseSwapchainImage");
            quad.space = r.space; quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
            quad.subImage.swapchain = r.swap;
            quad.subImage.imageRect.extent = {640, 360};
            quad.pose.orientation.w = 1; quad.pose.position.z = -2;
            quad.size = {1.6f, 0.9f};
        }
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};
        end.displayTime = state.predictedDisplayTime;
        end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end.layerCount = state.shouldRender && !exit_requested ? 1 : 0;
        end.layers = end.layerCount ? &layer : nullptr;
        xr_check(xrEndFrame(s.session, &end), "xrEndFrame");
        ++frames; layers_sent += end.layerCount;
    }
    throw std::runtime_error("XR session did not start/stop within the experiment deadline");
}
