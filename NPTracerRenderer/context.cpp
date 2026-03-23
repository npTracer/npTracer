#define VMA_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "context.h"
#include <stb_image.h>

#include <algorithm>
#include <optional>
#include <cstring>
#include <chrono>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// -----------------------------------------------------------------------------
// VULKAN
// -----------------------------------------------------------------------------

void Context::createWindow(GLFWwindow*& window, int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
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

    Queue graphicsQueue;
    Queue transferQueue;

    // queue selection
    for (uint32_t i = 0; i < properties.size(); i++)
    {
        const auto& family = properties[i];

        VkBool32 presentSupport;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
        {
            graphicsQueue.index = i;
        }
        else if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            transferQueue.index = i;
        }
    }

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    if (graphicsQueue)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                 .queueFamilyIndex = graphicsQueue.index.value(),
                                                 .queueCount = 1,
                                                 .pQueuePriorities = &queuePriority };
        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    if (transferQueue)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                 .queueFamilyIndex = transferQueue.index.value(),
                                                 .queueCount = 1,
                                                 .pQueuePriorities = &queuePriority };
        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    // TODO add device features
    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures2 deviceFeatures2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                               .pNext = &vulkan13Features,
                                               .features = { .samplerAnisotropy = true } };

    // TODO add more device extensions
    std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create logical device");
    }

    if (graphicsQueue)
    {
        vkGetDeviceQueue(device, graphicsQueue.index.value(), 0, &graphicsQueue.queue);
        VkCommandPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = graphicsQueue.index.value() };

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsQueue.commandPool)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool");
        }
        queues[QueueFamily::GRAPHICS] = graphicsQueue;
    }

    if (transferQueue)
    {
        vkGetDeviceQueue(device, transferQueue.index.value(), 0, &transferQueue.queue);
        VkCommandPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = transferQueue.index.value() };

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &transferQueue.commandPool)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool");
        }
        queues[QueueFamily::TRANSFER] = transferQueue;
    }

    queueFamilyIndices = {
        queues[QueueFamily::GRAPHICS].index.value(),
        queues[QueueFamily::TRANSFER].index.value(),
    };
}

void Context::createAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo{
        .physicalDevice = physicalDevice,
        .device = device,
        .instance = instance,
        .vulkanApiVersion = VK_API_VERSION_1_3,
    };

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vma allocator!");
    }
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
    VkShaderModule coreVertModule = createShaderModule(readFile(NPTRACER_SHADER_CORE_VERT));
    VkShaderModule coreFragModule = createShaderModule(readFile(NPTRACER_SHADER_CORE_FRAG));

    VkPipelineShaderStageCreateInfo vInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = coreVertModule,
        .pName = "vertMain"
    };

    VkPipelineShaderStageCreateInfo fInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = coreFragModule,
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

    VkVertexInputBindingDescription bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()
    };

    VkPipelineInputAssemblyStateCreateInfo inputInfo{
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
    VkPipelineViewportStateCreateInfo viewportState{
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
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
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

    // depth testing
    VkPipelineDepthStencilStateCreateInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };

    // pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0
    };
    vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    VkPipelineRenderingCreateInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainParams.format.format,
        .depthAttachmentFormat = swapchainParams.depthFormat
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
        .pDepthStencilState = &depthInfo,
        .pColorBlendState = &blendStateInfo,
        .pDynamicState = &dynamicInfo,
        .layout = pipelineLayout,
        .renderPass = nullptr
    };

    if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &pipeline)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline");
    }

    vkDestroyShaderModule(device, coreVertModule, nullptr);
    vkDestroyShaderModule(device, coreFragModule, nullptr);
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
        Frame frame;

        vkCreateSemaphore(device, &semInfo, nullptr, &frame.donePresentingSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &frame.doneExecutingFence);
        createCommandBuffer(frame.commandBuffer, QueueFamily::GRAPHICS);

        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        createBuffer(frame.uboBuffer, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                         | VMA_ALLOCATION_CREATE_MAPPED_BIT);

        frames.emplace_back(frame);
    }

    // create transfer command buffer as well
    createCommandBuffer(transferCommandBuffer, QueueFamily::TRANSFER);
}

void Context::createRenderingResources()
{
    VkDeviceSize vertexBufferSize = sizeof(vertices[0]) * vertices.size();
    createDeviceLocalBuffer(vertexBuffer, (void*)vertices.data(), vertexBufferSize,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VkDeviceSize indexBufferSize = sizeof(indices[0]) * indices.size();
    createDeviceLocalBuffer(indexBuffer, (void*)indices.data(), indexBufferSize,
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    createTextureImage();
    createTextureSampler();
}

// -----------------------------------------------------------------------------
// COMMAND BUFFERS
// -----------------------------------------------------------------------------

void Context::createCommandBuffer(VkCommandBuffer& commandBuffer, QueueFamily queueFamily)
{
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool = queues[queueFamily].commandPool,
                                           .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = 1 };

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffer");
    }
}

void Context::beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                        .flags = flags,
                                        .pInheritanceInfo = nullptr };

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer");
    }
}

void Context::endCommandBuffer(VkCommandBuffer commandBuffer, QueueFamily queueFamily)
{
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    vkQueueSubmit(queues[queueFamily].queue, 1, &submitInfo, nullptr);
}

// -----------------------------------------------------------------------------
// BUFFERS
// -----------------------------------------------------------------------------

void Context::createBuffer(Buffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags allocationFlags)
{
    VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = size,
                                   .usage = usage,
                                   .sharingMode = VK_SHARING_MODE_CONCURRENT,
                                   .queueFamilyIndexCount = 2,
                                   .pQueueFamilyIndices = queueFamilyIndices.data() };

    VmaAllocationCreateInfo allocCreateInfo{ .flags = allocationFlags,
                                             .usage = VMA_MEMORY_USAGE_AUTO };

    // allocate buffer memory (to be filled in with memcpy)
    if (vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &handle.buffer,
                        &handle.allocation, &handle.allocInfo)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create buffer!");
    }
}

void Context::createDeviceLocalBuffer(Buffer& handle, void* data, VkDeviceSize size,
                                      VkBufferUsageFlags usage)
{
    Buffer stagingBuffer;
    createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(stagingBuffer.allocInfo.pMappedData, data, size);

    createBuffer(handle, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);

    copyBuffer(stagingBuffer, handle, size);

    vkQueueWaitIdle(queues[QueueFamily::TRANSFER].queue);
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

void Context::copyBuffer(Buffer& src, Buffer& dst, VkDeviceSize size)
{
    vkResetCommandBuffer(transferCommandBuffer, 0);
    beginCommandBuffer(transferCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    VkBufferCopy bufferCopy{ 0, 0, size };
    vkCmdCopyBuffer(transferCommandBuffer, src.buffer, dst.buffer, 1, &bufferCopy);
    endCommandBuffer(transferCommandBuffer, QueueFamily::TRANSFER);
}

// -----------------------------------------------------------------------------
// IMAGES
// -----------------------------------------------------------------------------

void Context::createImage(Image& handle, VkImageType type, VkFormat format, uint32_t width,
                          uint32_t height, VkImageUsageFlags usage,
                          VmaAllocationCreateFlags allocationFlags)
{
    VkImageCreateInfo imageInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .imageType = type,
                                 .format = format,
                                 .extent = { width, height, 1 },
                                 .mipLevels = 1,
                                 .arrayLayers = 1,
                                 .samples = VK_SAMPLE_COUNT_1_BIT,
                                 .tiling = VK_IMAGE_TILING_OPTIMAL,
                                 .usage = usage,
                                 .sharingMode = VK_SHARING_MODE_EXCLUSIVE };

    VmaAllocationCreateInfo allocCreateInfo{ .flags = allocationFlags,
                                             .usage = VMA_MEMORY_USAGE_AUTO };

    // allocate buffer memory (to be filled in with memcpy)
    if (vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &handle.image, &handle.allocation,
                       &handle.allocInfo)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!");
    }
}

void Context::createTextureImage()
{
    int width, height, channels;
    auto path = TEXTURE("coconut.jpg");
    stbi_uc* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if (!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    Buffer stagingBuffer;
    VkDeviceSize size = width * height * 4;
    createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(stagingBuffer.allocInfo.pMappedData, pixels, size);

    stbi_image_free(pixels);

    createImage(textureImage, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, width, height,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(commandBuffer, QueueFamily::GRAPHICS);
    beginCommandBuffer(commandBuffer);

    transitionImageLayout(commandBuffer, textureImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    copyBufferToImage(commandBuffer, stagingBuffer, textureImage, width, height);

    transitionImageLayout(commandBuffer, textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                          VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    endCommandBuffer(commandBuffer, QueueFamily::GRAPHICS);

    // view creation
    VkImageViewCreateInfo viewInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                    .image = textureImage.image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = VK_FORMAT_R8G8B8A8_SRGB,
                                    .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

    vkCreateImageView(device, &viewInfo, nullptr, &textureImage.view);

    vkQueueWaitIdle(queues[QueueFamily::GRAPHICS].queue);
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

void Context::createDepthImage()
{
    std::vector<VkFormat> candidates{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                      VK_FORMAT_D24_UNORM_S8_UINT };

    swapchainParams.depthFormat = VK_FORMAT_UNDEFINED;
    for (VkFormat candidate : candidates)
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &properties);

        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            swapchainParams.depthFormat = candidate;
            break;
        }
    }

    if (swapchainParams.depthFormat == VK_FORMAT_UNDEFINED)
    {
        throw std::runtime_error("failed to find supported depth format");
    }

    createImage(depthImage, VK_IMAGE_TYPE_2D, swapchainParams.depthFormat,
                swapchainParams.extent.width, swapchainParams.extent.height,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(commandBuffer, QueueFamily::GRAPHICS);
    beginCommandBuffer(commandBuffer);
    transitionImageLayout(commandBuffer, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                              | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                          VK_IMAGE_ASPECT_DEPTH_BIT);

    endCommandBuffer(commandBuffer, QueueFamily::GRAPHICS);

    VkImageViewCreateInfo viewInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                    .image = depthImage.image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = swapchainParams.depthFormat,
                                    .subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 } };

    vkCreateImageView(device, &viewInfo, nullptr, &depthImage.view);
}

void Context::createTextureSampler()
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{ .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .anisotropyEnable = VK_TRUE,
                                     .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
                                     .compareEnable = VK_FALSE,
                                     .compareOp = VK_COMPARE_OP_ALWAYS,
                                     .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE };

    vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
}

void Context::copyBufferToImage(VkCommandBuffer commandBuffer, Buffer& src, Image& dst,
                                uint32_t width, uint32_t height)
{
    VkBufferImageCopy region
    {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = 
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1}
    };

    vkCmdCopyBufferToImage(commandBuffer, src.buffer, dst.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void Context::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                    VkImageLayout oldLayout, VkImageLayout newLayout,
                                    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                                    VkPipelineStageFlags2 srcStageMask,
                                    VkPipelineStageFlags2 dstStageMask,
                                    VkImageAspectFlags aspectFlags)
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
                                      .subresourceRange = { .aspectMask = aspectFlags,
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

// -----------------------------------------------------------------------------
// DESCRIPTORS
// -----------------------------------------------------------------------------

void Context::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{
        { { .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
            .pImmutableSamplers = nullptr },
          { .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr } }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
}

void Context::createDescriptorPool()
{
    std::array<VkDescriptorPoolSize, 2> poolSizes{
        { { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = FRAME_COUNT },
          { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = FRAME_COUNT } }
    };

    VkDescriptorPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                         .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                         .maxSets = FRAME_COUNT,
                                         .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                         .pPoolSizes = poolSizes.data() };

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
}

void Context::createDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(FRAME_COUNT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = descriptorPool,
                                           .descriptorSetCount = static_cast<uint32_t>(
                                               layouts.size()),
                                           .pSetLayouts = layouts.data() };

    std::vector<VkDescriptorSet> sets(FRAME_COUNT);
    vkAllocateDescriptorSets(device, &allocInfo, sets.data());

    std::vector<VkDescriptorBufferInfo> bufferInfos(FRAME_COUNT);
    std::vector<VkDescriptorImageInfo> imageInfos(FRAME_COUNT);
    std::vector<VkWriteDescriptorSet> descriptorWrites(FRAME_COUNT * 2);
    for (uint32_t i = 0; i < FRAME_COUNT; i++)
    {
        frames[i].descriptorSet = sets[i];

        bufferInfos[i] = { .buffer = frames[i].uboBuffer.buffer,
                           .offset = 0,
                           .range = sizeof(UniformBufferObject) };

        imageInfos[i] = { .sampler = textureSampler,
                          .imageView = textureImage.view,
                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        descriptorWrites[2 * i + 0] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                        .dstSet = frames[i].descriptorSet,
                                        .dstBinding = 0,
                                        .dstArrayElement = 0,
                                        .descriptorCount = 1,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                        .pBufferInfo = &bufferInfos[i] };

        descriptorWrites[2 * i + 1] = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                        .dstSet = frames[i].descriptorSet,
                                        .dstBinding = 1,
                                        .dstArrayElement = 0,
                                        .descriptorCount = 1,
                                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        .pImageInfo = &imageInfos[i] };
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()),
                           descriptorWrites.data(), 0, nullptr);
}

// -----------------------------------------------------------------------------
// DRAW CALL
// -----------------------------------------------------------------------------

void Context::executeDrawCall(GLFWwindow* window)
{
    // grab a frame
    Frame& frame = frames[currentFrame];

    // wait until this frame has finished executing its commands
    vkWaitForFences(device, 1, &frame.doneExecutingFence, VK_TRUE, UINT64_MAX);

    // acquire image when it is done being presented
    uint32_t imageIndex;
    if (vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, frame.donePresentingSemaphore, nullptr,
                              &imageIndex)
        == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain(window);
        return;
    }

    populateDrawCall(frame.commandBuffer,
                     imageIndex);  // record commands into frame's command buffer
    vkResetFences(device, 1, &frame.doneExecutingFence);  // signal that fence is ready to be
                                                          // associated with a new queue submission

    VkPipelineStageFlags waitDestinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores
        = &frame.donePresentingSemaphore,  // wait to submit work until image is done being presented
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &doneRenderingSemaphores[imageIndex]
    };  // signal that rendering is finished once execution is finished

    vkQueueSubmit(queues[QueueFamily::GRAPHICS].queue, 1, &submitInfo,
                  frame.doneExecutingFence);  // signal that execution has been completed on frame
                                              // once it is done

    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores
        = &doneRenderingSemaphores[imageIndex],  // submit for presentation once rendering is finished
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &imageIndex
    };

    VkResult result = vkQueuePresentKHR(queues[QueueFamily::GRAPHICS].queue, &presentInfo);
    if ((result == VK_SUBOPTIMAL_KHR) || (result == VK_ERROR_OUT_OF_DATE_KHR) || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapchain(window);
    }

    // increment frame (within ring)
    currentFrame = (currentFrame + 1) % FRAME_COUNT;
}

void Context::populateDrawCall(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    vkResetCommandBuffer(commandBuffer, 0);
    beginCommandBuffer(commandBuffer);

    transitionImageLayout(commandBuffer, swapchainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {},
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkClearValue clearColor{ .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkClearValue clearDepth{ .depthStencil = { 1.0f, 0 } };

    VkRenderingAttachmentInfo colorAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchainImageViews[imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearColor
    };

    VkRenderingAttachmentInfo depthAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImage.view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clearDepth
    };

    VkRenderingInfo renderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                                   .renderArea = { .offset = { 0, 0 },
                                                   .extent = swapchainParams.extent },
                                   .layerCount = 1,
                                   .colorAttachmentCount = 1,
                                   .pColorAttachments = &colorAttachmentInfo,
                                   .pDepthAttachment = &depthAttachmentInfo };

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

    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                            &frames[currentFrame].descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(commandBuffer, indices.size(), 1, 0, 0, 0);

    vkCmdEndRendering(commandBuffer);

    transitionImageLayout(commandBuffer, swapchainImages[imageIndex],
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, {},
                          VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);

    vkEndCommandBuffer(commandBuffer);
}

void Context::updateUniformBuffer()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime)
                     .count();

    UniformBufferObject ubo{
        .model = rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        .view = lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                       glm::vec3(0.0f, 0.0f, 1.0f)),
        .proj = glm::perspective(glm::radians(45.0f),
                                 static_cast<float>(swapchainParams.extent.width)
                                     / static_cast<float>(swapchainParams.extent.height),
                                 0.1f, 10.0f)
    };
    ubo.proj[1][1] *= -1;

    memcpy(frames[currentFrame].uboBuffer.allocInfo.pMappedData, &ubo, sizeof(UniformBufferObject));
}

// -----------------------------------------------------------------------------
// UTILITY
// -----------------------------------------------------------------------------

void Context::recreateSwapchain(GLFWwindow* window)
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    createSwapchain(window);
    createSwapchainImageViews();
    createDepthImage();
}

void Context::cleanupSwapchain()
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(swapchainImageViews.size()); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    swapchainImageViews.clear();
    depthImage.destroy(device, allocator);

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void Context::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* context = reinterpret_cast<Context*>(glfwGetWindowUserPointer(window));
    context->framebufferResized = true;
}

VkShaderModule Context::createShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo sci{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                  .codeSize = code.size() * sizeof(char),
                                  .pCode = reinterpret_cast<const uint32_t*>(code.data()) };

    VkShaderModule sm;
    vkCreateShaderModule(device, &sci, nullptr, &sm);
    return sm;
}

void Context::waitIdle()
{
    vkDeviceWaitIdle(device);
}

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

void Context::destroy()
{
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        vkDestroySemaphore(device, doneRenderingSemaphores[i], nullptr);
    }

    for (int i = 0; i < FRAME_COUNT; i++)
    {
        frames[i].destroy(device, allocator);
    }

    depthImage.destroy(device, allocator);
    textureImage.destroy(device, allocator);
    vkDestroySampler(device, textureSampler, nullptr);

    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);

    for (auto& queue : queues)
    {
        queue.second.destroy(device);
    }

    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
    }

    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }

    cleanupSwapchain();

    vertexBuffer.destroy(allocator);
    indexBuffer.destroy(allocator);

    vmaDestroyAllocator(allocator);

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