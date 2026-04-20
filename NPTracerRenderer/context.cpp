#include "context.h"
#include "utils.h"

#include <glm/glm.hpp>
#include <stb_image.h>

#include <algorithm>
#include <optional>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

NP_TRACER_NAMESPACE_BEGIN

void Context::createWindow(GLFWwindow*& window, uint32_t width, uint32_t height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, sFramebufferResizeCallback);
}

// VULKAN
void Context::createInstance(bool enableDebug)
{
    // glfw extensions
    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);  // required for swapchain extension

    if (enableDebug)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // layers
    std::vector<const char*> layers;
    if (enableDebug)
    {
        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
        layers.assign(validationLayers.begin(), validationLayers.end());
    }

    // TODO poll if layers/extensions are supported

    VkApplicationInfo appInfo{ .pApplicationName = "Engine",
                               .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                               .pEngineName = "Bum",
                               .engineVersion = VK_MAKE_VERSION(1, 0, 0),
                               .apiVersion = VK_API_VERSION_1_4 };

    VkInstanceCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &appInfo,
                                     .enabledExtensionCount = static_cast<uint32_t>(
                                         extensions.size()),
                                     .ppEnabledExtensionNames = extensions.data() };

    // declare `debugCreateInfo` outside of if statement so it stays alive until `vkCreateInstance`
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableDebug)
    {  // create a separate debug messenger for instance creation, as debug messenger creation depends on instance creation

        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        sPopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;

        /* enable specific validation features */
        VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
            VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
        };
        VkValidationFeaturesEXT validationFeatures = {
            .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
            .enabledValidationFeatureCount = std::size(enabledValidationFeatures),
            .pEnabledValidationFeatures = enabledValidationFeatures
        };
        debugCreateInfo.pNext = &validationFeatures;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance), "instance creation failed\n");

    if (enableDebug)
    {  // create debug messenger after instance creation
        createDebugMessenger(enableDebug);
    }
}

void Context::createPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for (const auto& dev : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(dev, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(dev, &deviceFeatures);

        if (deviceProperties.deviceType
            == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)  // TODO better selection criteria
        {
            physicalDevice = dev;
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

    // queue selection
    for (uint32_t i = 0; i < properties.size(); i++)
    {
        const auto& family = properties[i];

        if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT
            && !queues.contains(QueueType::GRAPHICS))  // do not overwrite with a later match
        {
            queues[QueueType::GRAPHICS].index = i;  // operator[] will insert if does not exist
        }
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (!queues.contains(QueueType::TRANSFER)
                || !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {  // allow overwrite if this is a dedicated transfer queue
                queues[QueueType::TRANSFER].index = i;
            }
        }
    }

    std::unordered_set<uint32_t> queueFamilyIndicesSet;
    for (auto it = queues.begin(); it != queues.end();)
    {
        if (!it->second) it = queues.erase(it);  // should not occur, but for completion's sake
        else
        {
            queueFamilyIndicesSet.insert(it->second.index.value());
            ++it;
        }
    }
    queueFamilyIndices.clear();
    queueFamilyIndices.reserve(static_cast<size_t>(QueueType::_COUNT));  // reserve upfront
    queueFamilyIndices.assign(queueFamilyIndicesSet.begin(), queueFamilyIndicesSet.end());

    constexpr float kQueuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(queueFamilyIndices.size());
    for (uint32_t idx : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                 .queueFamilyIndex = idx,
                                                 .queueCount = 1,
                                                 .pQueuePriorities = &kQueuePriority };

        queueCreateInfos.push_back(queueCreateInfo);
    }

    // TODO add more device extensions
    std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME,
    };

    // probe for support
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asProbe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtProbe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &asProbe
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaProbe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, .pNext = &rtProbe
    };

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferProbe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT, .pNext = &bdaProbe
    };

    VkPhysicalDeviceDescriptorIndexingFeatures indexingProbe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &descriptorBufferProbe
    };

    VkPhysicalDeviceVulkan13Features vulkan13Probe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &indexingProbe
    };

    VkPhysicalDeviceVulkan11Features vulkan11Probe{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, .pNext = &vulkan13Probe
    };

    VkPhysicalDeviceFeatures2 features2Probe{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                              .pNext = &vulkan11Probe };

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2Probe);

    bool valid = asProbe.accelerationStructure && rtProbe.rayTracingPipeline
                 && bdaProbe.bufferDeviceAddress && descriptorBufferProbe.descriptorBuffer
                 && indexingProbe.shaderSampledImageArrayNonUniformIndexing
                 && indexingProbe.descriptorBindingStorageBufferUpdateAfterBind
                 && indexingProbe.descriptorBindingUpdateUnusedWhilePending
                 && indexingProbe.descriptorBindingPartiallyBound
                 && indexingProbe.descriptorBindingVariableDescriptorCount
                 && indexingProbe.runtimeDescriptorArray && vulkan13Probe.synchronization2
                 && vulkan13Probe.dynamicRendering && vulkan11Probe.shaderDrawParameters
                 && features2Probe.features.shaderInt64
                 && features2Probe.features.samplerAnisotropy;

    DEV_ASSERT(valid, "required features not supported\n");

    // enable features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure = VK_TRUE
    };

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
        .pNext = &asFeatures,
        .rayTracingPipeline = VK_TRUE
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = &rtFeatures,
        .bufferDeviceAddress = VK_TRUE
    };

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT,
        .pNext = &bdaFeatures,
        .descriptorBuffer = VK_TRUE
    };

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &descriptorBufferFeatures,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features vulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &indexingFeatures,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceVulkan11Features vulkan11Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &vulkan13Features,
        .shaderDrawParameters = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                         .pNext = &vulkan11Features };
    features2.features.shaderInt64 = VK_TRUE;
    features2.features.samplerAnisotropy = true;

    VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size()),
        .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
        .pEnabledFeatures = nullptr
    };

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
             "failed to create logical device\n");
    loadRayTracingFunctionPointers();

    for (auto& queue : queues | std::views::values)  // values() of map
    {
        if (!queue) continue;
        vkGetDeviceQueue(device, queue.index.value(), 0, &queue.queue);

        VkCommandPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                          .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                          .queueFamilyIndex = queue.index.value() };

        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &queue.commandPool),
                 "failed to create command pool\n");
    }

    // query acceleration structure properties
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 props2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                                        .pNext = &accelProps };

    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    scratchAlignment = accelProps.minAccelerationStructureScratchOffsetAlignment;
}

void Context::createAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo{ .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                          .physicalDevice = physicalDevice,
                                          .device = device,
                                          .instance = instance,
                                          .vulkanApiVersion = VK_API_VERSION_1_3 };

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator), "failed to create vma allocator!\n");
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
        return format.format == VK_FORMAT_R8G8B8A8_SRGB
               && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    };

    auto formatIt = std::ranges::find_if(availableFormats.begin(), availableFormats.end(),
                                         is_format);

    VkSurfaceFormatKHR format = formatIt != availableFormats.end() ? *formatIt
                                                                   : availableFormats[0];

    // present selection
    auto presentIt = std::ranges::find(presentModes.begin(), presentModes.end(),
                                       VK_PRESENT_MODE_MAILBOX_KHR);

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
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                      | VK_IMAGE_USAGE_STORAGE_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain),
             "failed to create swapchain\n");

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    // save for later
    swapchainParams = { .surfaceFormat = format, .presentMode = presentMode, .extent = extent };

    VkImageViewCreateInfo imageViewCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                               .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                               .format = swapchainParams.surfaceFormat.format,
                                               .subresourceRange = {
                                                   .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                   .baseMipLevel = 0,
                                                   .levelCount = 1,
                                                   .baseArrayLayer = 0,
                                                   .layerCount = 1 } };

    for (auto& image : swapchainImages)
    {
        imageViewCreateInfo.image = image;
        VkImageView view;
        vkCreateImageView(device, &imageViewCreateInfo, nullptr, &view);
        swapchainImageViews.emplace_back(view);
    }
}

void Context::recreateSwapchain(GLFWwindow* window)
{
    int width = 0, height = 0;
    do
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    } while (width == 0 || height == 0);

    waitIdle();
    cleanupSwapchain();
    createSwapchain(window);
    createDepthImage(width, height);
    createResultImages(width, height);

    std::vector<Image> resultImages{ resultImage, accumulationImage };
    writeDescriptorSetImages(rtDescriptorSet, 1, resultImages, VK_NULL_HANDLE,
                             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
    frameIndex.store(0);  // reset atomic
}

void Context::cleanupSwapchain()
{
    for (uint32_t i = 0; i < static_cast<uint32_t>(swapchainImageViews.size()); i++)
    {
        vkDestroyImageView(device, swapchainImageViews[i], nullptr);
    }
    swapchainImageViews.clear();

    if (depthImage.image != VK_NULL_HANDLE)
    {
        depthImage.destroy(device, allocator);
    }

    if (resultImage.image != VK_NULL_HANDLE)
    {
        resultImage.destroy(device, allocator);
    }

    if (accumulationImage.image != VK_NULL_HANDLE)
    {
        accumulationImage.destroy(device, allocator);
    }

    if (swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void Context::createSyncAndFrameObjects(size_t numRenderingSemaphores)
{
    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                 .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    doneRenderingSemaphores.reserve(numRenderingSemaphores);
    for (int i = 0; i < numRenderingSemaphores; i++)
    {
        VkSemaphore doneRenderingSemaphore;
        vkCreateSemaphore(device, &semInfo, nullptr, &doneRenderingSemaphore);
        doneRenderingSemaphores.push_back(doneRenderingSemaphore);
    }

    frames.reserve(kFramesInFlight);

    for (int i = 0; i < kFramesInFlight; i++)
    {
        Frame& frame = frames[i];

        vkCreateSemaphore(device, &semInfo, nullptr, &frame.donePresentingSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &frame.doneExecutingFence);
        createCommandBuffer(&frame.commandBuffer, QueueType::GRAPHICS);
    }

    // create transfer command buffer as well
    createCommandBuffer(&transferCommandBuffer, QueueType::TRANSFER);
}

void Context::createSurface(GLFWwindow* window)
{
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface),
             "surface creation failed\n");
}

// COMMAND BUFFERS
void Context::createCommandBuffer(VkCommandBuffer* pOutCommandBuffer, QueueType queueFamily)
{
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool = queues[queueFamily].commandPool,
                                           .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                           .commandBufferCount = 1 };

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, pOutCommandBuffer),
             "failed to allocate command buffer\n");
}

void Context::sBeginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                        .flags = flags,
                                        .pInheritanceInfo = nullptr };

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo),
             "failed to begin recording command buffer\n");
}

void Context::submitCommandBuffer(VkCommandBuffer commandBuffer, QueueType queueFamily,
                                  VkPipelineStageFlags waitDstFlags, VkFence fence,
                                  VkSemaphore waitSemaphores, VkSemaphore signalSemaphores)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                             .waitSemaphoreCount = waitSemaphores ? 1u : 0u,
                             .pWaitSemaphores = &waitSemaphores,
                             .pWaitDstStageMask = &waitDstFlags,
                             .commandBufferCount = 1u,
                             .pCommandBuffers = &commandBuffer,
                             .signalSemaphoreCount = signalSemaphores ? 1u : 0u,
                             .pSignalSemaphores = &signalSemaphores };

    VK_CHECK(vkQueueSubmit(queues[queueFamily].queue, 1, &submitInfo, fence),
             "failed to submit command buffer\n");
}

void Context::freeCommandBuffer(VkCommandBuffer commandBuffer, QueueType queueFamily)
{
    if (commandBuffer == VK_NULL_HANDLE) return;

    vkFreeCommandBuffers(device, queues[queueFamily].commandPool, 1, &commandBuffer);
}

// BUFFERS
bool Context::createBuffer(Buffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags allocationFlags) const
{
    VkBufferCreateInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                   .size = size,
                                   .usage = usage,
                                   .sharingMode = VK_SHARING_MODE_CONCURRENT,
                                   .queueFamilyIndexCount = 2,
                                   .pQueueFamilyIndices = queueFamilyIndices.data() };

    VmaAllocationCreateInfo allocCreateInfo{ .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
                                                      | allocationFlags,
                                             .usage = VMA_MEMORY_USAGE_AUTO };

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &handle.buffer,
                        &handle.allocation, &handle.allocInfo)
        != VK_SUCCESS)
    {
        DBG_PRINT("failed to create device local buffer!\n");
        return false;
    }

    return true;
}

bool Context::createDeviceLocalBuffer(Buffer& handle, const void* data, VkDeviceSize size,
                                      VkBufferUsageFlags usage)
{
    Buffer stagingBuffer;
    if (!createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                          | VMA_ALLOCATION_CREATE_MAPPED_BIT))
    {
        DBG_PRINT("failed to create staging buffer for device local buffer!\n");
        return false;
    }

    memcpy(stagingBuffer.allocInfo.pMappedData, data, size);

    if (!createBuffer(handle, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0))
    {
        DBG_PRINT("failed to create device local buffer!\n");
        vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);
        return false;
    }

    copyBuffer(stagingBuffer, handle, size);

    vkQueueWaitIdle(queues[QueueType::TRANSFER].queue);
    stagingBuffer.destroy(allocator);

    return true;
}

void Context::copyBuffer(const Buffer& src, const Buffer& dst, VkDeviceSize size)
{
    vkResetCommandBuffer(transferCommandBuffer, 0);
    sBeginCommandBuffer(transferCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy bufferCopy{ .srcOffset = 0, .dstOffset = 0, .size = size };
    vkCmdCopyBuffer(transferCommandBuffer, src.buffer, dst.buffer, 1, &bufferCopy);

    submitCommandBuffer(transferCommandBuffer, QueueType::TRANSFER);
}

VkDeviceAddress Context::getBufferDeviceAddress(const Buffer& buffer) const
{
    VkBufferDeviceAddressInfo addressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                           .buffer = buffer.buffer };

    return vkGetBufferDeviceAddress(device, &addressInfo);
}

// IMAGES
void Context::createImage(Image* pOutHandle, VkImageType type, VkFormat format, uint32_t width,
                          uint32_t height, VkImageUsageFlags usage,
                          VmaAllocationCreateFlags allocationFlags, VkImageAspectFlags aspect,
                          bool shouldCreateView) const
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

    VK_CHECK(vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &pOutHandle->image,
                            &pOutHandle->allocation, &pOutHandle->allocInfo),
             "failed to create image!\n");

    pOutHandle->width = width;
    pOutHandle->height = height;
    pOutHandle->format = format;

    if (!shouldCreateView)
    {
        return;  // good to return early here
    }

    // create view
    VkImageViewCreateInfo viewInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                    .image = pOutHandle->image,
                                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                    .format = pOutHandle->format,
                                    .subresourceRange = { .aspectMask = aspect,
                                                          .baseMipLevel = 0,
                                                          .levelCount = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount = 1 } };

    vkCreateImageView(device, &viewInfo, nullptr, &pOutHandle->view);
}

void Context::createTextureImage(Image* pOutHandle, const void* pPixels, uint32_t width,
                                 uint32_t height, VkFormat format)
{
    Buffer stagingBuffer;
    VkDeviceSize size = width * height * 4;
    createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(stagingBuffer.allocInfo.pMappedData, pPixels, size);

    createImage(pOutHandle, VK_IMAGE_TYPE_2D, format, width, height,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(&commandBuffer, QueueType::GRAPHICS);
    sBeginCommandBuffer(commandBuffer);

    pOutHandle->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                                 VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                 VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT);

    sCopyBufferToImage(pOutHandle, stagingBuffer, commandBuffer, width, height);

    pOutHandle->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                 VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                 VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                 VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    submitCommandBuffer(commandBuffer, QueueType::GRAPHICS);

    vkQueueWaitIdle(queues[QueueType::GRAPHICS].queue);
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    pOutHandle->width = width;
    pOutHandle->height = height;
    pOutHandle->format = format;
}

void Context::createDepthImage(uint32_t width, uint32_t height)
{
    std::vector<VkFormat> candidates{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT,
                                      VK_FORMAT_D24_UNORM_S8_UINT };

    depthFormat = VK_FORMAT_UNDEFINED;
    for (VkFormat candidate : candidates)
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, candidate, &properties);

        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = candidate;
            break;
        }
    }

    DEV_ASSERT(depthFormat != VK_FORMAT_UNDEFINED, "failed to create depth image!\n");

    createImage(&depthImage, VK_IMAGE_TYPE_2D, depthFormat, width, height,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(&commandBuffer, QueueType::GRAPHICS);
    sBeginCommandBuffer(commandBuffer);
    sTransitionImageLayout(commandBuffer, depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                               | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
                               | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                           VK_IMAGE_ASPECT_DEPTH_BIT);

    submitCommandBuffer(commandBuffer, QueueType::GRAPHICS);
}

void Context::createResultImages(uint32_t width, uint32_t height)
{
    std::vector<Image*> handles{ &resultImage, &accumulationImage };

    VkCommandBuffer commandBuffer;
    createCommandBuffer(&commandBuffer, QueueType::GRAPHICS);
    sBeginCommandBuffer(commandBuffer);

    for (uint32_t i = 0; i < static_cast<uint32_t>(handles.size()); i++)
    {
        // result image
        createImage(handles[i], VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, width, height,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_STORAGE_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);

        sTransitionImageLayout(commandBuffer, handles[i]->image, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                               VK_IMAGE_ASPECT_COLOR_BIT);
    }

    submitCommandBuffer(commandBuffer, QueueType::GRAPHICS);
    vkQueueWaitIdle(queues[QueueType::GRAPHICS].queue);
}

void Context::createTextureSampler(VkSampler* pOutSampler) const
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

    vkCreateSampler(device, &samplerInfo, nullptr, pOutSampler);
}

void Context::sCopyBufferToImage(Image* pOutDst, const Buffer& src, VkCommandBuffer commandBuffer,
                                 uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{ .bufferOffset = 0,
                              .bufferRowLength = 0,
                              .bufferImageHeight = 0,
                              .imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                    .mipLevel = 0,
                                                    .baseArrayLayer = 0,
                                                    .layerCount = 1 },
                              .imageOffset = { 0, 0, 0 },
                              .imageExtent = { width, height, 1 } };

    vkCmdCopyBufferToImage(commandBuffer, src.buffer, pOutDst->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    pOutDst->width = width;
    pOutDst->height = height;
}

void Context::sCopyImageToBuffer(const Buffer* pOutDstHandle, const Image& src,
                                 VkCommandBuffer commandBuffer, uint32_t width, uint32_t height,
                                 VkImageAspectFlags aspectFlags)
{
    VkBufferImageCopy region{ .bufferOffset = 0,
                              .bufferRowLength = 0,
                              .bufferImageHeight = 0,
                              .imageSubresource = { .aspectMask = aspectFlags,
                                                    .mipLevel = 0,
                                                    .baseArrayLayer = 0,
                                                    .layerCount = 1 },
                              .imageExtent = { width, height, 1 } };

    vkCmdCopyImageToBuffer(commandBuffer, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           pOutDstHandle->buffer, 1, &region);
}

void Context::sTransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                     VkImageLayout oldLayout, VkImageLayout newLayout,
                                     VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                                     VkPipelineStageFlags2 srcStageMask,
                                     VkPipelineStageFlags2 dstStageMask,
                                     VkImageAspectFlags aspectFlags)
{
    VkImageMemoryBarrier2 barrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
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

void Context::createBottomLevelAccelerationStructure(AccelerationStructure* pOutHandle,
                                                     VkCommandBuffer commandBuffer,
                                                     VkDeviceAddress vertexAddress,
                                                     VkDeviceAddress indexAddress,
                                                     uint32_t firstVertex, uint32_t vertexCount,
                                                     uint32_t firstIndex, uint32_t indexCount) const
{
    DEV_ASSERT(indexCount != 0 && (indexCount % 3) == 0,
               "BLAS build requires a non-zero triangle index count.\n");

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = vertexAddress },
        .vertexStride = sizeof(Vertex),
        .maxVertex = firstVertex + vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = indexAddress },
        .transformData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    const uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{
        .primitiveCount = primitiveCount,
        .primitiveOffset = static_cast<uint32_t>(firstIndex * sizeof(uint32_t)),
        .firstVertex = 0,
        .transformOffset = 0
    };

    // BUILD
    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &geometryInfo, &primitiveCount, &sizeInfo);

    // allocate scratch buffer
    VkDeviceSize scratchSize = sizeInfo.buildScratchSize + scratchAlignment - 1;

    createBuffer(pOutHandle->scratchBuffer, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);
    VkDeviceAddress rawScratchAddress = getBufferDeviceAddress(pOutHandle->scratchBuffer);
    VkDeviceAddress scratchAddress = alignUpVk(rawScratchAddress, scratchAlignment);

    // allocate blas storage buffer
    createBuffer(pOutHandle->handleBuffer, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 0);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = pOutHandle->handleBuffer.buffer,
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VK_CHECK(vkCreateAccelerationStructureKHR(device, &createInfo, nullptr,
                                              &pOutHandle->accelerationStructure),
             "failed to create acceleration structure!\n");

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = pOutHandle->accelerationStructure,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry,
        .scratchData = { .deviceAddress = scratchAddress }
    };

    // build it

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildRangeInfo);
}

void Context::createTopLevelAccelerationStructure(AccelerationStructure* pOutAccelStructHandle,
                                                  Buffer* pOutInstanceBufferHandle,
                                                  VkCommandBuffer commandBuffer,
                                                  const std::vector<FLOAT4x4>& transforms,
                                                  const std::vector<AccelerationStructure>& blasses)
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(transforms.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(transforms.size()); i++)
    {
        VkAccelerationStructureInstanceKHR instance{
            .transform = toVkTransform(transforms[i]),
            .instanceCustomIndex = static_cast<uint32_t>(i),
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasses[i].deviceAddress
        };

        instances.push_back(instance);
    }

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());
    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    DEV_ASSERT(
        createDeviceLocalBuffer(*pOutInstanceBufferHandle, instances.data(), instanceBufferSize,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT),
        "failed to create device local memory buffer!\n");

    VkDeviceAddress instanceBufferAddress = getBufferDeviceAddress(*pOutInstanceBufferHandle);

    VkAccelerationStructureGeometryDataKHR geometry{
        .instances = { .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                       .arrayOfPointers = VK_FALSE,
                       .data = { .deviceAddress = instanceBufferAddress } }
    };

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = geometry,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &geometryInfo, &instanceCount, &sizeInfo);

    createBuffer(pOutAccelStructHandle->handleBuffer, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 0);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = pOutAccelStructHandle->handleBuffer.buffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    vkCreateAccelerationStructureKHR(device, &createInfo, nullptr,
                                     &pOutAccelStructHandle->accelerationStructure);

    // allocate scratch buffer
    VkDeviceSize scratchSize = sizeInfo.buildScratchSize + scratchAlignment - 1;

    createBuffer(pOutAccelStructHandle->scratchBuffer, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);
    VkDeviceAddress rawScratchAddress = getBufferDeviceAddress(pOutAccelStructHandle->scratchBuffer);
    VkDeviceAddress scratchAddress = alignUpVk(rawScratchAddress, scratchAlignment);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{ .primitiveCount = instanceCount,
                                                             .primitiveOffset = 0,
                                                             .firstVertex = 0,
                                                             .transformOffset = 0 };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = pOutAccelStructHandle->accelerationStructure,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry,
        .scratchData = { .deviceAddress = scratchAddress }
    };

    // build it
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildRangeInfo);
}

void Context::createDescriptorSetLayout(DescriptorSetLayout* pOutDescriptorSetLayout,
                                        const std::vector<VkDescriptorSetLayoutBinding>& bindings,
                                        const std::vector<VkDescriptorBindingFlags>* pBindingFlags,
                                        VkDescriptorSetLayoutCreateFlags layoutCreateFlags,
                                        VkDescriptorPoolCreateFlags poolCreateFlags) const
{
    const uint32_t kBindingsCount = bindings.size();

    // method-level static is necessary as 0 is 0 AND moreover we need the pointer address to exist in the scope of the function call
    static std::vector<VkDescriptorBindingFlags> pFallbackBindingFlags;
    if (pBindingFlags)  // if was passed in, check it
        DEV_ASSERT(pBindingFlags->size() == kBindingsCount, "flags should match bindings in count");
    else pFallbackBindingFlags.resize(kBindingsCount, 0);

    // create info for bindless descriptor sets
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
        .bindingCount = kBindingsCount,
        .pBindingFlags = pBindingFlags ? pBindingFlags->data() : pFallbackBindingFlags.data(),
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .flags = layoutCreateFlags,
        .bindingCount = kBindingsCount,
        .pBindings = bindings.data()
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                         &pOutDescriptorSetLayout->layout),
             "failed to create mesh descriptor set layout\n");

    // get binding types and number
    std::unordered_map<VkDescriptorType, uint32_t> countMap;
    for (auto& binding : bindings)
    {
        countMap[binding.descriptorType] += binding.descriptorCount;
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto& pair : countMap)
    {
        poolSizes.push_back({ .type = pair.first, .descriptorCount = pair.second });
    }

    VkDescriptorPoolCreateInfo descriptorPoolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | poolCreateFlags,  // union
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };

    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr,
                                    &pOutDescriptorSetLayout->pool),
             "failed to create mesh descriptor pool\n");
}

void Context::allocateDescriptorSet(VkDescriptorSet* pOutDescriptorSet,
                                    DescriptorSetLayout& descriptorSetLayout) const
{
    VkDescriptorSetAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                           .descriptorPool = descriptorSetLayout.pool,
                                           .descriptorSetCount = 1,
                                           .pSetLayouts = &descriptorSetLayout.layout };

    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, pOutDescriptorSet),
             "failed to allocate mesh descriptor set\n");
}

void Context::writeDescriptorSetBuffers(
    const VkDescriptorSet& descriptorSet, const std::vector<Buffer*>& bindingBuffers,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) const
{
    std::vector<VkDescriptorBufferInfo> bindingInfos;
    for (auto& buf : bindingBuffers)
    {
        bindingInfos.push_back({ .buffer = buf->buffer, .offset = 0, .range = VK_WHOLE_SIZE });
    }

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (uint32_t i = 0; i < bindingInfos.size(); ++i)
    {
        const VkDescriptorSetLayoutBinding& binding = bindings[i];
        VkWriteDescriptorSet writeDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                 .dstSet = descriptorSet,
                                                 .dstBinding = binding.binding,
                                                 .descriptorCount = binding.descriptorCount,
                                                 .descriptorType = binding.descriptorType,
                                                 .pBufferInfo = &bindingInfos[i] };

        writeDescriptorSets.push_back(writeDescriptorSet);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
}

void Context::writeDescriptorSetImages(const VkDescriptorSet& descriptorSet, uint32_t binding,
                                       const std::vector<Image>& images, VkSampler inSampler,
                                       VkDescriptorType type, VkImageLayout layout) const
{
    std::vector<VkDescriptorImageInfo> imageInfos;
    VkSampler s = (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ? inSampler : VK_NULL_HANDLE;
    for (auto& image : images)
    {
        VkDescriptorImageInfo imageInfo{ .sampler = s,
                                         .imageView = image.view,
                                         .imageLayout = layout };
        imageInfos.push_back(imageInfo);
    }

    VkWriteDescriptorSet writeDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                             .dstSet = descriptorSet,
                                             .dstBinding = binding,
                                             .descriptorCount = static_cast<uint32_t>(
                                                 imageInfos.size()),
                                             .descriptorType = type,
                                             .pImageInfo = imageInfos.data() };

    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void Context::writeDescriptorSetAccelerationStructures(
    const VkDescriptorSet& descriptorSet,
    const std::vector<AccelerationStructure*>& bindingAccelStructs,
    const std::vector<VkDescriptorSetLayoutBinding>& bindings) const
{
    std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelStructsInfo;
    for (auto& pair : bindingAccelStructs)
    {
        accelStructsInfo.push_back(
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
              .accelerationStructureCount = 1,
              .pAccelerationStructures = &pair->accelerationStructure });
    }

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (uint32_t i = 0; i < accelStructsInfo.size(); ++i)
    {
        const auto& binding = bindings[i];
        VkWriteDescriptorSet writeDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                                 .pNext = &accelStructsInfo[i],
                                                 .dstSet = descriptorSet,
                                                 .dstBinding = i,
                                                 .descriptorCount = binding.descriptorCount,
                                                 .descriptorType = binding.descriptorType };

        writeDescriptorSets.push_back(writeDescriptorSet);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
}

// UTILITY
Frame& Context::getCurrentFrame(uint32_t currentFrame)
{
    return frames[currentFrame];
}

void Context::loadRayTracingFunctionPointers()
{
    vkCreateAccelerationStructureKHR = sLoadDeviceFunction<PFN_vkCreateAccelerationStructureKHR>(
        device, instance, "vkCreateAccelerationStructureKHR");

    vkDestroyAccelerationStructureKHR = sLoadDeviceFunction<PFN_vkDestroyAccelerationStructureKHR>(
        device, instance, "vkDestroyAccelerationStructureKHR");

    vkGetAccelerationStructureBuildSizesKHR = sLoadDeviceFunction<
        PFN_vkGetAccelerationStructureBuildSizesKHR>(device, instance,
                                                     "vkGetAccelerationStructureBuildSizesKHR");

    vkCmdBuildAccelerationStructuresKHR = sLoadDeviceFunction<
        PFN_vkCmdBuildAccelerationStructuresKHR>(device, instance,
                                                 "vkCmdBuildAccelerationStructuresKHR");

    vkGetAccelerationStructureDeviceAddressKHR
        = sLoadDeviceFunction<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            device, instance, "vkGetAccelerationStructureDeviceAddressKHR");

    vkCreateRayTracingPipelinesKHR
        = sLoadDeviceFunction<PFN_vkCreateRayTracingPipelinesKHR>(device, instance,
                                                                  "vkCreateRayTracingPipelinesKHR");

    vkGetRayTracingShaderGroupHandlesKHR = sLoadDeviceFunction<
        PFN_vkGetRayTracingShaderGroupHandlesKHR>(device, instance,
                                                  "vkGetRayTracingShaderGroupHandlesKHR");

    vkCmdTraceRaysKHR = sLoadDeviceFunction<PFN_vkCmdTraceRaysKHR>(device, instance,
                                                                   "vkCmdTraceRaysKHR");

    bool pointers = vkCreateAccelerationStructureKHR && vkDestroyAccelerationStructureKHR
                    && vkGetAccelerationStructureBuildSizesKHR
                    && vkCmdBuildAccelerationStructuresKHR
                    && vkGetAccelerationStructureDeviceAddressKHR && vkCreateRayTracingPipelinesKHR
                    && vkGetRayTracingShaderGroupHandlesKHR && vkCmdTraceRaysKHR;

    DEV_ASSERT(pointers, "failed to load ray tracing Vulkan function pointers\n");
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

void Context::waitIdle() const
{
    vkDeviceWaitIdle(device);
}

void Context::destroyDebugMessenger()
{
    if (debugMessenger == VK_NULL_HANDLE)
    {
        return;
    }
    const auto fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (fn) fn(instance, debugMessenger, nullptr);

    debugMessenger = VK_NULL_HANDLE;
}

void Context::destroy()
{
    cleanupSwapchain();

    for (int i = 0; i < kFramesInFlight; i++)
    {
        frames[i].destroy(device, allocator);
    }

    for (auto& sem : doneRenderingSemaphores)
    {
        vkDestroySemaphore(device, sem, nullptr);
    }
    doneRenderingSemaphores.clear();

    if (transferCommandBuffer != VK_NULL_HANDLE)
    {
        freeCommandBuffer(transferCommandBuffer, QueueType::TRANSFER);
    }

    for (auto& val : queues | std::views::values)
    {
        val.destroy(device);
    }

    if (allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(allocator);
        allocator = VK_NULL_HANDLE;
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

void Context::createDebugMessenger(bool enableDebug)
{
    if (!enableDebug)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    sPopulateDebugMessengerCreateInfo(createInfo);

    // `vkCreateDebugUtilsMessengerEXT` extension relies on a valid instance to have been created
    auto fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    DEV_ASSERT(fn, "debug layer function proc addr not found\n");

    fn(instance, &createInfo, nullptr, &debugMessenger);
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::sDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    DBG_PRINT("validation: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
}

void Context::sPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = VkDebugUtilsMessengerCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                       | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = sDebugCallback,
        .pUserData = nullptr
    };
}

void Context::sFramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* context = static_cast<Context*>(glfwGetWindowUserPointer(window));
    context->framebufferResized = true;
}

NP_TRACER_NAMESPACE_END
