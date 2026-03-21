#include "context.h"
#include <optional>
#include <algorithm>

void Context::createDebugMessenger(bool enableDebug)
{
    if (!enableDebug)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debugCallback,
        .pUserData = nullptr
    };

    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (fn)
    {
        fn(instance, &createInfo, nullptr, &debugMessenger);
    }
    else
    {
        throw std::runtime_error("debug layer not function proc addr not found");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << "validation: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Context::destroyDebugMessenger()
{
    if (debugMessenger == VK_NULL_HANDLE)
    {
        return;
    }
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (fn)
    {
        fn(instance, debugMessenger, nullptr);
    }

    debugMessenger = VK_NULL_HANDLE;
}

void Context::createWindow(GLFWwindow*& window, int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);
}

void Context::createInstance(bool enableDebug)
{
    // glfw extensions
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    if (enableDebug)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // layers
    std::vector<char const*> layers;
    if (enableDebug)
    {
        const std::vector<char const*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        layers.assign(validationLayers.begin(), validationLayers.end());
    }

    // TODO poll if layers/extensions are supported

    constexpr VkApplicationInfo appInfo{ .pApplicationName = "Engine",
                                         .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .pEngineName = "Bum",
                                         .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                                         .apiVersion = VK_API_VERSION_1_4 };

    VkInstanceCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &appInfo,
                                     .enabledLayerCount = static_cast<uint32_t>(layers.size()),
                                     .ppEnabledLayerNames = layers.data(),
                                     .enabledExtensionCount = static_cast<uint32_t>(
                                         extensions.size()),
                                     .ppEnabledExtensionNames = extensions.data() };

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("instance creation failed");
    }

    if (enableDebug)
    {
        createDebugMessenger(enableDebug);
    }
}

void Context::createSurface(GLFWwindow* window)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("surface creation failed");
    }
}

void Context::createPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        if (deviceProperties.deviceType
            == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)  // TODO better selection criteria
        {
            physicalDevice = device;
            break;
        }
    }
}

void Context::createLogicalDeviceAndQueues()
{
    uint32_t propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, nullptr);
    std::vector<VkQueueFamilyProperties> properties(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &propertyCount, properties.data());

    std::optional<uint32_t> graphicsIndex;
    for (uint32_t i = 0; i < properties.size(); i++)
    {
        const auto& family = properties[i];

        VkBool32 presentSupport;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
        {
            graphicsIndex = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                             .queueFamilyIndex = graphicsIndex.value(),
                                             .queueCount = 1,
                                             .pQueuePriorities = &queuePriority };

    // TODO add device features
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, 
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE, 
    };

    VkPhysicalDeviceFeatures deviceFeatures
    {
    };

    // TODO add more device extensions
    std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vulkan13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures,
    };

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }

    graphicsQueueFamilyIndex = graphicsIndex.value();
    vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);
}

void Context::createSwapchain(GLFWwindow* window)
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                         availableFormats.data());

    uint32_t presentCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount,
                                              presentModes.data());

    // format selection
    auto is_format = [](const VkSurfaceFormatKHR& format)
    {
        return format.format == VK_FORMAT_B8G8R8_SRGB
               && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    };
    auto formatIt = std::ranges::find_if(availableFormats, is_format);

    VkSurfaceFormatKHR format = formatIt != availableFormats.end() ? *formatIt
                                                                   : availableFormats[0];

    // present selection
    auto presentIt = std::ranges::find(presentModes, VK_PRESENT_MODE_MAILBOX_KHR);
    VkPresentModeKHR presentMode = presentIt != presentModes.end() ? *presentIt
                                                                   : VK_PRESENT_MODE_FIFO_KHR;

    // extent configuration
    VkExtent2D extent;

    if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        extent = surfaceCapabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        extent = { std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width,
                                        surfaceCapabilities.maxImageExtent.width),
                   std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height,
                                        surfaceCapabilities.maxImageExtent.height) };
    }

    // swapchain creation
    uint32_t minImageCount = std::max(3u, surfaceCapabilities.minImageCount);

    if ((0 < surfaceCapabilities.minImageCount)
        && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = minImageCount,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = true,
        .oldSwapchain = nullptr
    };

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
    {
        throw std::runtime_error("failed to create swapchain");
    }

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    // save for later
    swapchainParams = { .format = format, .presentMode = presentMode, .extent = extent };
}

void Context::createSwapchainImageViews() 
{
    VkImageViewCreateInfo imageViewCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                               .format = swapchainParams.format.format,
                                               .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                                     1, 0, 1 } };

    for (auto& image : swapchainImages)
    {
        imageViewCreateInfo.image = image;
        VkImageView view;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &view);
        swapchainImageViews.emplace_back(view);
    }
}

void Context::createGraphicsPipeline() 
{
    // shader creation
    VkShaderModule shaderModule = createShaderModule(readFile(NPTRACER_SHADER_PATH));

    VkPipelineShaderStageCreateInfo vInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = shaderModule,
        .pName = "vertMain"
    };

    VkPipelineShaderStageCreateInfo fInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = shaderModule,
        .pName = "fragMain"
    };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vInfo, fInfo };

    // viewport
    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()
    };

    VkPipelineVertexInputStateCreateInfo vertexInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo inputInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport viewport{ 0.0f,
                         0.0f,
                         static_cast<float>(swapchainParams.extent.width),
                         static_cast<float>(swapchainParams.extent.height),
                         0.0f,
                         1.0f };
    VkRect2D rect{ VkOffset2D{ 0, 0 }, swapchainParams.extent };
    VkPipelineViewportStateCreateInfo viewportState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasSlopeFactor = 1.0f,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    // color blending
    VkPipelineColorBlendAttachmentState blendInfo{ .blendEnable = VK_FALSE,
                                                   .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                                                                     | VK_COLOR_COMPONENT_G_BIT
                                                                     | VK_COLOR_COMPONENT_B_BIT
                                                                     | VK_COLOR_COMPONENT_A_BIT };
    VkPipelineColorBlendStateCreateInfo blendStateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blendInfo
    };

    // pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pushConstantRangeCount = 0
    };
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkPipelineRenderingCreateInfo renderingInfo 
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainParams.format.format
    };

    VkGraphicsPipelineCreateInfo pipelineInfo{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInfo,
        .pInputAssemblyState = &inputInfo,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterInfo,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &blendStateInfo,
        .pDynamicState = &dynamicInfo,
        .layout = pipelineLayout,
        .renderPass = nullptr
    };

    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);
}

void Context::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                      .queueFamilyIndex = graphicsQueueFamilyIndex };
    
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create command pool");
    }
}

void Context::createCommandBuffer(VkCommandBuffer& commandBuffer)
{
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool = commandPool,
                                           .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = 1 };

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffer");
    }
}

void Context::createSyncAndFrameObjects() 
{
    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                 .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        VkSemaphore doneRenderingSemaphore;
        vkCreateSemaphore(device, &semInfo, nullptr, &doneRenderingSemaphore);
        doneRenderingSemaphores.emplace_back(doneRenderingSemaphore);
    }

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        VkSemaphore donePresentingSemaphore;
        VkFence doneExecutingFence;
        VkCommandBuffer commandBuffer;

        vkCreateSemaphore(device, &semInfo, nullptr, &donePresentingSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &doneExecutingFence);
        createCommandBuffer(commandBuffer);

        donePresentingSemaphores.emplace_back(donePresentingSemaphore);
        doneExecutingFences.emplace_back(doneExecutingFence);
        commandBuffers.emplace_back(commandBuffer);
    }
}

void Context::beginCommandBuffer(VkCommandBuffer commandBuffer) 
{
    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                        .flags = 0,
                                        .pInheritanceInfo = nullptr };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer");
    }
}

void Context::recordRenderingCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);
    beginCommandBuffer(commandBuffer);

    transitionImageLayout(commandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearColorValue clearColor = VkClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f };
    VkRenderingAttachmentInfo attachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor
    };

    VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                   .renderArea = { .offset = { 0, 0 },
                                                   .extent = swapchainParams.extent },
                                   .layerCount = 1,
                                   .colorAttachmentCount = 1,
                                   .pColorAttachments = &attachmentInfo };

    // record commands
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport viewport{
        0.0f, 0.0f, (float)swapchainParams.extent.width, (float)swapchainParams.extent.height,
        0.0f, 1.0f
    };

    VkRect2D scissor{ { 0, 0 }, swapchainParams.extent };

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
    
    transitionImageLayout(commandBuffer, swapchainImages[imageIndex],
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

void Context::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                    VkImageLayout oldLayout,
                                    VkImageLayout newLayout, VkAccessFlags2 srcAccessMask,
                                    VkAccessFlags2 dstAccessMask,
                                    VkPipelineStageFlags2 srcStageMask,
                                    VkPipelineStageFlags2 dstStageMask)
{
    VkImageMemoryBarrier2 barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                      .srcStageMask = srcStageMask,
                                      .srcAccessMask = srcAccessMask,
                                      .dstStageMask = dstStageMask,
                                      .dstAccessMask = dstAccessMask,
                                      .oldLayout = oldLayout,
                                      .newLayout = newLayout,
                                      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                      .image = image,
                                      .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                            .baseMipLevel = 0,
                                                            .levelCount = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount = 1 } };

    VkDependencyInfo dependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                     .dependencyFlags = {},
                                     .imageMemoryBarrierCount = 1,
                                     .pImageMemoryBarriers = &barrier };

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void Context::drawFrame() 
{
    // grab a frame
    currentFrame = (currentFrame + 1) % FRAME_COUNT;

    // wait until this frame has finished executing its commands
    vkWaitForFences(device, 1, &doneExecutingFences[currentFrame], VK_TRUE, UINT64_MAX);

    // acquire image when it is done being presented
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, donePresentingSemaphores[currentFrame], nullptr,
                          &imageIndex);

    recordRenderingCommands(commandBuffers[currentFrame], imageIndex); // record commands into frame's command buffer 
    vkResetFences(device, 1, &doneExecutingFences[currentFrame]); // signal that fence is ready to be associated with a new queue submission
    
    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .waitSemaphoreCount = 1,
                             .pWaitSemaphores = &donePresentingSemaphores[currentFrame], // wait to submit work until image is done being presented
                             .pWaitDstStageMask = &waitDestinationStageMask,
                             .commandBufferCount = 1,
                             .pCommandBuffers = &commandBuffers[currentFrame],
                             .signalSemaphoreCount = 1,
                             .pSignalSemaphores = &doneRenderingSemaphores[imageIndex] }; // signal that rendering is finished once execution is finished

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, doneExecutingFences[currentFrame]); // signal that execution has been completed on frame once it is done

    VkPresentInfoKHR presentInfo{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                  .waitSemaphoreCount = 1,
                                  .pWaitSemaphores = &doneRenderingSemaphores[imageIndex], // submit for presentation once rendering is finished
                                  .swapchainCount = 1,
                                  .pSwapchains = &swapchain,
                                  .pImageIndices = &imageIndex };

    vkQueuePresentKHR(graphicsQueue, &presentInfo);
}

void Context::waitIdle() 
{
    vkDeviceWaitIdle(device);
}

VkShaderModule Context::createShaderModule(const std::vector<char>& code) const 
{
    VkShaderModuleCreateInfo sci
    {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size() * sizeof(char),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule sm;
    vkCreateShaderModule(device, &sci, nullptr, &sm);
    return sm;
}

void Context::destroy()
{
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroySemaphore(device, doneRenderingSemaphores[i], nullptr);
    }

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        vkDestroyFence(device, doneExecutingFences[i], nullptr);
        vkDestroySemaphore(device, donePresentingSemaphores[i], nullptr);
    }

    if (commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(swapchainImageViews.size()); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
    }

    if (device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    destroyDebugMessenger();

    if (instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
}
