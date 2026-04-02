#define VMA_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "context.h"
#include "utils.h"

#include <glm/glm.hpp>
#include <stb_image.h>

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

void Context::createWindow(GLFWwindow*& window, int width, int height)
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

    VkApplicationInfo appInfo{};
    appInfo.pApplicationName = "Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Bum";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // declare `debugCreateInfo` outside of if statement so it stays alive until `vkCreateInstance`
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if (enableDebug)
    {  // create a separate debug messenger for instance creation, as debug messenger creation depends on instance creation

        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        sPopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
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
            && !queues.count(NPQueueType::GRAPHICS))  // do not overwrite with a later match
        {
            queues[NPQueueType::GRAPHICS].index = i;  // operator[] will insert if does not exist
        }
        if (family.queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            if (!queues.count(NPQueueType::TRANSFER) || !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {  // allow overwrite if this is a dedicated transfer queue
                queues[NPQueueType::TRANSFER].index = i;
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
    queueFamilyIndices.reserve(static_cast<size_t>(NPQueueType::_COUNT));  // reserve upfront
    queueFamilyIndices.assign(queueFamilyIndicesSet.begin(), queueFamilyIndicesSet.end());

    constexpr float kQueuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(queueFamilyIndices.size());
    for (uint32_t idx : queueFamilyIndices)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = idx;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &kQueuePriority;

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
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asProbe{};
    asProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtProbe{};
    rtProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtProbe.pNext = &asProbe;

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaProbe{};
    bdaProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bdaProbe.pNext = &rtProbe;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferProbe{};
    descriptorBufferProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferProbe.pNext = &bdaProbe;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingProbe{};
    indexingProbe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingProbe.pNext = &descriptorBufferProbe;

    VkPhysicalDeviceVulkan13Features vulkan13Probe{};
    vulkan13Probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Probe.pNext = &indexingProbe;

    VkPhysicalDeviceVulkan11Features vulkan11Probe{};
    vulkan11Probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Probe.pNext = &vulkan13Probe;

    VkPhysicalDeviceFeatures2 features2Probe{};
    features2Probe.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2Probe.pNext = &vulkan11Probe;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &features2Probe);

    bool valid = asProbe.accelerationStructure && rtProbe.rayTracingPipeline
                 && bdaProbe.bufferDeviceAddress && descriptorBufferProbe.descriptorBuffer
                 && indexingProbe.shaderSampledImageArrayNonUniformIndexing
                 && indexingProbe.runtimeDescriptorArray
                 && indexingProbe.descriptorBindingPartiallyBound && vulkan13Probe.synchronization2
                 && vulkan13Probe.dynamicRendering && vulkan11Probe.shaderDrawParameters
                 && features2Probe.features.shaderInt64
                 && features2Probe.features.samplerAnisotropy;

    DEV_ASSERT(valid, "required features not supported");

    // enable features
    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.rayTracingPipeline = VK_TRUE;
    rtFeatures.pNext = &asFeatures;

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bdaFeatures.bufferDeviceAddress = VK_TRUE;
    bdaFeatures.pNext = &rtFeatures;

    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures{};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
    descriptorBufferFeatures.pNext = &bdaFeatures;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexingFeatures.pNext = &descriptorBufferFeatures;
    indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    indexingFeatures.runtimeDescriptorArray = VK_TRUE;
    indexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;

    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.pNext = &indexingFeatures;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.pNext = &vulkan13Features;
    vulkan11Features.shaderDrawParameters = VK_TRUE;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &vulkan11Features;
    features2.features.shaderInt64 = VK_TRUE;
    features2.features.samplerAnisotropy = true;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &features2;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
    createInfo.pEnabledFeatures = nullptr;

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device),
             "failed to create logical device\n");
    loadRayTracingFunctionPointers();

    for (auto& [type, queue] : queues)
    {
        if (!queue) continue;
        vkGetDeviceQueue(device, queue.index.value(), 0, &queue.queue);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queue.index.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &queue.commandPool) != VK_SUCCESS)
        {
            vkGetDeviceQueue(device, queue.index.value(), 0, &queue.queue);

            VkCommandPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            poolInfo.queueFamilyIndex = queue.index.value();

            if (vkCreateCommandPool(device, &poolInfo, nullptr, &queue.commandPool) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create command pool\n");
            }
        }
    }

    // query acceleration structure properties
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelProps{};
    accelProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &accelProps;

    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    scratchAlignment = accelProps.minAccelerationStructureScratchOffsetAlignment;
}

void Context::createAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create vma allocator!\n");
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

    auto formatIt = std::find_if(availableFormats.begin(), availableFormats.end(), is_format);

    VkSurfaceFormatKHR format = formatIt != availableFormats.end() ? *formatIt
                                                                   : availableFormats[0];

    // present selection
    auto presentIt = std::find(presentModes.begin(), presentModes.end(),
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

    VkSwapchainCreateInfoKHR swapchainCreateInfo{};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.surface = surface;
    swapchainCreateInfo.minImageCount = minImageCount;
    swapchainCreateInfo.imageFormat = format.format;
    swapchainCreateInfo.imageColorSpace = format.colorSpace;
    swapchainCreateInfo.imageExtent = extent;
    swapchainCreateInfo.imageArrayLayers = 1;
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                     | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
    {
        throw std::runtime_error("failed to create swapchain");
    }

    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    swapchainImages.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());

    // save for later
    swapchainParams = { format, presentMode, extent };

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = swapchainParams.surfaceFormat.format;

    imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = 1;
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

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
    createResultImages();

    std::vector<NPImage> resultImages{ resultImage, accumulationImage };
    writeDescriptorSetImages(rtDescriptorSet, 1, resultImages, VK_NULL_HANDLE,
                             VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_IMAGE_LAYOUT_GENERAL);
    frameIndex = 0;
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

void Context::createSyncAndFrameObjects()
{
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    const size_t kNumRenderingSemaphores = swapchainImages.size();
    doneRenderingSemaphores.reserve(kNumRenderingSemaphores);
    for (int i = 0; i < kNumRenderingSemaphores; i++)
    {
        VkSemaphore doneRenderingSemaphore;
        vkCreateSemaphore(device, &semInfo, nullptr, &doneRenderingSemaphore);
        doneRenderingSemaphores.push_back(doneRenderingSemaphore);
    }

    frames.reserve(kFrameCount);

    for (int i = 0; i < kFrameCount; i++)
    {
        NPFrame& frame = frames[i];

        vkCreateSemaphore(device, &semInfo, nullptr, &frame.donePresentingSemaphore);
        vkCreateFence(device, &fenceInfo, nullptr, &frame.doneExecutingFence);
        createCommandBuffer(frame.commandBuffer, NPQueueType::GRAPHICS);
    }

    // create transfer command buffer as well
    createCommandBuffer(transferCommandBuffer, NPQueueType::TRANSFER);
}

void Context::createSurface(GLFWwindow* window)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("surface creation failed");
    }
}

// COMMAND BUFFERS
void Context::createCommandBuffer(VkCommandBuffer& commandBuffer, NPQueueType queueFamily)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = queues[queueFamily].commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer),
             "failed to allocate command buffer\n");
}

void Context::beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo),
             "failed to begin recording command buffer\n");
}

void Context::endCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily,
                               VkPipelineStageFlags waitDstFlags, VkFence fence,
                               VkSemaphore waitSemaphores, VkSemaphore signalSemaphores)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitDstFlags;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = waitSemaphores ? 1 : 0;
    submitInfo.pWaitSemaphores = &waitSemaphores;
    submitInfo.signalSemaphoreCount = signalSemaphores ? 1 : 0;
    submitInfo.pSignalSemaphores = &signalSemaphores;

    VK_CHECK(vkQueueSubmit(queues[queueFamily].queue, 1, &submitInfo, fence),
             "failed to submit command buffer\n");
}

void Context::freeCommandBuffer(VkCommandBuffer commandBuffer, NPQueueType queueFamily)
{
    if (commandBuffer == VK_NULL_HANDLE) return;

    vkFreeCommandBuffers(device, queues[queueFamily].commandPool, 1, &commandBuffer);
}

// BUFFERS
bool Context::createBuffer(NPBuffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags allocationFlags) const
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
    bufferInfo.queueFamilyIndexCount = 2;
    bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    // `VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT` required with `VMA_MEMORY_USAGE_AUTO`
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | allocationFlags;

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &handle.buffer,
                        &handle.allocation, &handle.allocInfo)
        != VK_SUCCESS)
    {
        DBG_PRINT("failed to create device local buffer!\n");
        return false;
    }

    return true;
}

bool Context::createDeviceLocalBuffer(NPBuffer& handle, const void* data, VkDeviceSize size,
                                      VkBufferUsageFlags usage)
{
    NPBuffer stagingBuffer;
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

    vkQueueWaitIdle(queues[NPQueueType::TRANSFER].queue);
    stagingBuffer.destroy(allocator);

    return true;
}

void Context::copyBuffer(NPBuffer& src, NPBuffer& dst, VkDeviceSize size)
{
    vkResetCommandBuffer(transferCommandBuffer, 0);
    beginCommandBuffer(transferCommandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkBufferCopy bufferCopy{ 0, 0, size };
    vkCmdCopyBuffer(transferCommandBuffer, src.buffer, dst.buffer, 1, &bufferCopy);

    endCommandBuffer(transferCommandBuffer, NPQueueType::TRANSFER);
}

VkDeviceAddress Context::getBufferDeviceAddress(NPBuffer& buffer)
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = buffer.buffer;

    return vkGetBufferDeviceAddress(device, &addressInfo);
}

// IMAGES
void Context::createImage(NPImage& handle, VkImageType type, VkFormat format, uint32_t width,
                          uint32_t height, VkImageUsageFlags usage,
                          VmaAllocationCreateFlags allocationFlags, VkImageAspectFlags aspect,
                          bool shouldCreateView) const
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = type;
    imageInfo.format = format;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo{};
    allocCreateInfo.flags = allocationFlags;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator, &imageInfo, &allocCreateInfo, &handle.image, &handle.allocation,
                       &handle.allocInfo)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create image!\n");
    }

    handle.width = width;
    handle.height = height;
    handle.format = format;

    if (!shouldCreateView)
    {
        return;  // good to return early here
    }

    // create view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = handle.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = handle.format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    vkCreateImageView(device, &viewInfo, nullptr, &handle.view);
}

void Context::createTextureImage(NPImage& handle, void* pixels, uint32_t width, uint32_t height)
{
    NPBuffer stagingBuffer;
    VkDeviceSize size = width * height * 4;
    createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(stagingBuffer.allocInfo.pMappedData, pixels, size);
    free(pixels);

    createImage(handle, VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_SRGB, width, height,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 0);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    beginCommandBuffer(commandBuffer);

    handle.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT);

    copyBufferToImage(commandBuffer, stagingBuffer, handle, width, height);

    handle.transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);

    endCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);

    vkQueueWaitIdle(queues[NPQueueType::GRAPHICS].queue);
    vmaDestroyBuffer(allocator, stagingBuffer.buffer, stagingBuffer.allocation);

    handle.width = width;
    handle.height = height;
    handle.format = VK_FORMAT_R8G8B8A8_SRGB;
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

    createImage(depthImage, VK_IMAGE_TYPE_2D, depthFormat, width, height,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, VK_IMAGE_ASPECT_DEPTH_BIT);

    VkCommandBuffer commandBuffer;
    createCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
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

    endCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
}

void Context::createResultImages()
{
    std::vector<NPImage*> handles{ &resultImage, &accumulationImage };

    VkCommandBuffer commandBuffer;
    createCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    beginCommandBuffer(commandBuffer);

    for (uint32_t i = 0; i < static_cast<uint32_t>(handles.size()); i++)
    {
        // result image
        createImage(*handles[i], VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
                    swapchainParams.extent.width, swapchainParams.extent.height,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                        | VK_IMAGE_USAGE_STORAGE_BIT,
                    VK_IMAGE_ASPECT_COLOR_BIT);

        transitionImageLayout(commandBuffer, handles[i]->image, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL, 0, VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                              VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                              VK_IMAGE_ASPECT_COLOR_BIT);
    }

    endCommandBuffer(commandBuffer, NPQueueType::GRAPHICS);
    vkQueueWaitIdle(queues[NPQueueType::GRAPHICS].queue);
}

void Context::createTextureSampler(VkSampler& sampler)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
}

void Context::copyBufferToImage(VkCommandBuffer commandBuffer, NPBuffer& src, NPImage& dst,
                                uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{};

    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, src.buffer, dst.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    dst.width = width;
    dst.height = height;
}

void Context::copyImageToBuffer(VkCommandBuffer commandBuffer, NPImage& src, NPBuffer& dst,
                                uint32_t width, uint32_t height, VkImageAspectFlags aspectFlags)
{
    VkBufferImageCopy region{};  // copy specifier
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = aspectFlags;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageExtent = { width, height, 1 };

    vkCmdCopyImageToBuffer(commandBuffer, src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           dst.buffer, 1, &region);
}

void Context::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                    VkImageLayout oldLayout, VkImageLayout newLayout,
                                    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                                    VkPipelineStageFlags2 srcStageMask,
                                    VkPipelineStageFlags2 dstStageMask,
                                    VkImageAspectFlags aspectFlags)
{
    VkImageMemoryBarrier2 barrier{};

    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStageMask;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;

    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkDependencyInfo dependencyInfo{};

    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.dependencyFlags = {};
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void Context::createBottomLevelAccelerationStructure(VkCommandBuffer& commandBuffer,
                                                     NPAccelerationStructure& handle,
                                                     VkDeviceAddress vertexAddress,
                                                     VkDeviceAddress indexAddress,
                                                     uint32_t firstVertex, uint32_t vertexCount,
                                                     uint32_t firstIndex, uint32_t indexCount)
{
    if (indexCount == 0 || (indexCount % 3) != 0)
    {
        throw std::runtime_error("BLAS build requires a non-zero triangle index count.");
    }

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(NPVertex);
    triangles.maxVertex = firstVertex + vertexCount - 1;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    triangles.transformData.deviceAddress = 0;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    accelerationStructureGeometry.geometry.triangles = triangles;

    const uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = primitiveCount;
    buildRangeInfo.primitiveOffset = firstIndex * sizeof(uint32_t);
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    // BUILD
    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{};
    geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    geometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = 1;
    geometryInfo.pGeometries = &accelerationStructureGeometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &geometryInfo, &primitiveCount, &sizeInfo);

    // allocate scratch buffer
    VkDeviceSize scratchSize = sizeInfo.buildScratchSize + scratchAlignment - 1;

    createBuffer(handle.scratchBuffer, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);
    VkDeviceAddress rawScratchAddress = getBufferDeviceAddress(handle.scratchBuffer);
    VkDeviceAddress scratchAddress = alignUpVk(rawScratchAddress, scratchAlignment);

    // allocate blas storage buffer
    createBuffer(handle.handleBuffer, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 0);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = handle.handleBuffer.buffer;
    createInfo.offset = 0;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    if (vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &handle.accelerationStructure)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create acceleration structure!");
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = handle.accelerationStructure;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // build it

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildRangeInfo);
}

void Context::createTopLevelAccelerationStructure(VkCommandBuffer& commandBuffer,
                                                  NPAccelerationStructure& handle,
                                                  NPBuffer& instanceBufferHandle,
                                                  std::vector<FLOAT4X4>& transforms,
                                                  std::vector<NPAccelerationStructure>& blasses)
{
    std::vector<VkAccelerationStructureInstanceKHR> instances;
    instances.reserve(transforms.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(transforms.size()); i++)
    {
        VkAccelerationStructureInstanceKHR instance{};
        instance.transform = toVkTransform(transforms[i]);
        instance.instanceCustomIndex = static_cast<uint32_t>(i);
        instance.mask = 0xFF;
        instance.instanceShaderBindingTableRecordOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance.accelerationStructureReference = blasses[i].deviceAddress;

        instances.push_back(instance);
    }

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());
    VkDeviceSize instanceBufferSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    if (!createDeviceLocalBuffer(instanceBufferHandle, instances.data(), instanceBufferSize,
                                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT))
    {
        throw std::runtime_error("failed to create device local memory buffer!");
    }

    VkDeviceAddress instanceBufferAddress = getBufferDeviceAddress(instanceBufferHandle);

    VkAccelerationStructureGeometryDataKHR geometry{};
    geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.instances.data.deviceAddress = instanceBufferAddress;

    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
    accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    accelerationStructureGeometry.geometry = geometry;
    accelerationStructureGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{};
    geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    geometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    geometryInfo.geometryCount = 1;
    geometryInfo.pGeometries = &accelerationStructureGeometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &geometryInfo, &instanceCount, &sizeInfo);

    createBuffer(handle.handleBuffer, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                     | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 0);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = handle.handleBuffer.buffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(device, &createInfo, nullptr, &handle.accelerationStructure);

    // allocate scratch buffer
    VkDeviceSize scratchSize = sizeInfo.buildScratchSize + scratchAlignment - 1;

    createBuffer(handle.scratchBuffer, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);
    VkDeviceAddress rawScratchAddress = getBufferDeviceAddress(handle.scratchBuffer);
    VkDeviceAddress scratchAddress = alignUpVk(rawScratchAddress, scratchAlignment);

    VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
    buildRangeInfo.primitiveCount = instanceCount;
    buildRangeInfo.primitiveOffset = 0;
    buildRangeInfo.firstVertex = 0;
    buildRangeInfo.transformOffset = 0;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = handle.accelerationStructure;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &accelerationStructureGeometry;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // build it
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfo = &buildRangeInfo;
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &buildInfo, &pBuildRangeInfo);
}

void Context::createDescriptorSetLayout(
    NPDescriptorSetLayout& descriptorSetLayout,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindings)
{
    std::vector<VkDescriptorSetLayoutBinding> bindingVec;
    bindingVec.reserve(bindings.size());

    for (auto& binding : bindings)
    {
        bindingVec.push_back(binding.second);
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindingVec.size());
    layoutInfo.pBindings = bindingVec.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout.layout)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create mesh descriptor set layout");
    }

    // get binding types and number
    std::unordered_map<VkDescriptorType, uint32_t> countMap;
    for (auto& binding : bindings)
    {
        countMap[binding.second.descriptorType] += binding.second.descriptorCount;
    }

    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto& pair : countMap)
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = pair.first;
        poolSize.descriptorCount = pair.second;
        poolSizes.push_back(poolSize);
    }

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 1;  // maybe change later

    if (vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorSetLayout.pool)
        != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create mesh descriptor pool");
    }
}

void Context::allocateDesciptorSet(VkDescriptorSet& descriptorSet,
                                   NPDescriptorSetLayout& descriptorSetLayout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorSetLayout.pool;
    allocInfo.descriptorSetCount = 1;  // maybe change later
    allocInfo.pSetLayouts = &descriptorSetLayout.layout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate mesh descriptor set");
    }
}

void Context::writeDescriptorSetBuffers(
    VkDescriptorSet& descriptorSet, std::unordered_map<uint32_t, NPBuffer*>& bindingBufferMap,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindingMap)
{
    std::unordered_map<uint32_t, VkDescriptorBufferInfo> bindingInfoMap;
    for (auto& pair : bindingBufferMap)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = pair.second->buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        bindingInfoMap[pair.first] = bufferInfo;
    }

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (auto& pair : bindingInfoMap)
    {
        uint32_t binding = pair.first;

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.descriptorCount = bindingMap[binding].descriptorCount;
        writeDescriptorSet.descriptorType = bindingMap[binding].descriptorType;
        writeDescriptorSet.pBufferInfo = &bindingInfoMap[binding];

        writeDescriptorSets.push_back(writeDescriptorSet);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
}

void Context::writeDescriptorSetImages(VkDescriptorSet& descriptorSet, uint32_t binding,
                                       const std::vector<NPImage>& images, VkSampler* sampler,
                                       VkDescriptorType type, VkImageLayout layout)
{
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (auto& image : images)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = image.view;
        imageInfo.imageLayout = layout;

        if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            imageInfo.sampler = *sampler;
        }

        imageInfos.push_back(imageInfo);
    }

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    writeDescriptorSet.descriptorType = type;
    writeDescriptorSet.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

void Context::writeDescriptorSetAccelerationStructures(
    VkDescriptorSet& descriptorSet,
    std::unordered_map<uint32_t, NPAccelerationStructure*>& bindingASMap,
    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding>& bindingMap)
{
    std::unordered_map<uint32_t, VkWriteDescriptorSetAccelerationStructureKHR> bindingInfoMap;
    for (auto& pair : bindingASMap)
    {
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount = 1;
        asInfo.pAccelerationStructures = &pair.second->accelerationStructure;

        bindingInfoMap[pair.first] = asInfo;
    }

    std::vector<VkWriteDescriptorSet> writeDescriptorSets;
    for (auto& pair : bindingInfoMap)
    {
        uint32_t binding = pair.first;

        VkWriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet = descriptorSet;
        writeDescriptorSet.dstBinding = binding;
        writeDescriptorSet.descriptorCount = bindingMap[binding].descriptorCount;
        writeDescriptorSet.descriptorType = bindingMap[binding].descriptorType;
        writeDescriptorSet.pNext = &pair.second;

        writeDescriptorSets.push_back(writeDescriptorSet);
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()),
                           writeDescriptorSets.data(), 0, nullptr);
}

// UTILITY
NPFrame& Context::getCurrentFrame(uint32_t currentFrame)
{
    return frames[currentFrame];
}

void Context::loadRayTracingFunctionPointers()
{
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));

    vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));

    vkGetAccelerationStructureBuildSizesKHR
        = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));

    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));

    vkGetAccelerationStructureDeviceAddressKHR
        = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));

    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));

    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));

    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));

    if (!vkCreateAccelerationStructureKHR || !vkDestroyAccelerationStructureKHR
        || !vkGetAccelerationStructureBuildSizesKHR || !vkCmdBuildAccelerationStructuresKHR
        || !vkGetAccelerationStructureDeviceAddressKHR || !vkCreateRayTracingPipelinesKHR
        || !vkGetRayTracingShaderGroupHandlesKHR || !vkCmdTraceRaysKHR)
    {
        throw std::runtime_error("failed to load ray tracing Vulkan function pointers");
    }
}

VkShaderModule Context::createShaderModule(const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    sci.codeSize = code.size() * sizeof(char);
    sci.pCode = reinterpret_cast<const uint32_t*>(code.data());

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

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    sPopulateDebugMessengerCreateInfo(createInfo);

    // `vkCreateDebugUtilsMessengerEXT` extension relies on a valid instance to have been created
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

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
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                                 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                             | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = sDebugCallback;
    createInfo.pUserData = nullptr;
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
    cleanupSwapchain();

    for (int i = 0; i < kFrameCount; i++)
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
        freeCommandBuffer(transferCommandBuffer, NPQueueType::TRANSFER);
    }

    for (auto& queue : queues)
    {
        queue.second.destroy(device);
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

void Context::sFramebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* context = static_cast<Context*>(glfwGetWindowUserPointer(window));
    context->framebufferResized = true;
}
