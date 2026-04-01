#define VMA_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "context.h"

#include "utils.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <optional>
#include <iostream>
#include <unordered_map>
#include <stb_image.h>

// #include "../../../../../../../Program Files/Side Effects
// Software/Houdini 21.0.596/toolkit/include/oneapi/tbb/detail/_task.h"
#include "external/assimp/code/AssetLib/3MF/3MFXmlTags.h"

void Context::createWindow(GLFWwindow*& window, int width, int height)
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, "Engine", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
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

    if (transferQueue)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = transferQueue.index.value();
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProperties{};
    descriptorBufferProperties.sType
        = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;

    VkPhysicalDeviceProperties2 properties2{};
    properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties2.pNext = &descriptorBufferProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &properties2);

    // TODO add device features
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.shaderDrawParameters = VK_TRUE;
    vulkan11Features.pNext = &vulkan13Features;

    VkPhysicalDeviceBufferDeviceAddressFeatures bdaFeatures{};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bdaFeatures.bufferDeviceAddress = VK_TRUE;
    bdaFeatures.pNext = &vulkan11Features;

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

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &indexingFeatures;
    features2.features.shaderInt64 = VK_TRUE;
    features2.features.samplerAnisotropy = true;

    // TODO add more device extensions
    std::vector<const char*> requiredDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                          VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
                                                          VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME };

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

    for (auto& [type, queue] : queues)
    {
        throw std::runtime_error("failed to create logical device");
    }

    if (graphicsQueue)
    {
        vkGetDeviceQueue(device, graphicsQueue.index.value(), 0, &graphicsQueue.queue);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueue.index.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &graphicsQueue.commandPool)
            != VK_SUCCESS)
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
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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
    imageViewCreateInfo.format = swapchainParams.format.format;

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
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    createSwapchain(window);
    createDepthImage(width, height);
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

    doneRenderingSemaphores.resize(swapchainImages.size());
    for (auto& sem : doneRenderingSemaphores)
    {
        vkCreateSemaphore(device, &semInfo, nullptr, &sem);
    }

    frames.resize(FRAME_COUNT);

    for (int i = 0; i < FRAME_COUNT; i++)
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
                               VkPipelineStageFlags waitDstFlags, VkFence fence)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pWaitDstStageMask = &waitDstFlags;
    submitInfo.pCommandBuffers = &commandBuffer;

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
        std::cout << "createDeviceLocalBuffer: failed to create staging buffer\n";
        return false;
    }

    memcpy(stagingBuffer.allocInfo.pMappedData, data, size);

    if (!createBuffer(handle, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0))
    {
        std::cout << "createDeviceLocalBuffer: failed to create device-local buffer\n";
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

void Context::createTextureImage(NPImage& handle, void* pixels, uint32_t width, uint32_t height,
                                 TextureOwnership ownership)
{
    NPBuffer stagingBuffer;
    VkDeviceSize size = width * height * 4;
    createBuffer(stagingBuffer, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT);

    memcpy(stagingBuffer.allocInfo.pMappedData, pixels, size);

    switch (ownership)
    {
        case TextureOwnership::STB: stbi_image_free(pixels); break;
        case TextureOwnership::MALLOC: free(pixels); break;
        case TextureOwnership::NONE: break;
    }
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
                                uint32_t width, uint32_t height)
{
    VkBufferImageCopy region{};  // copy specifier
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
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
                                       const std::vector<NPImage>& images, VkSampler& sampler)
{
    std::vector<VkDescriptorImageInfo> imageInfos;
    for (auto& image : images)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = image.view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = sampler;

        imageInfos.push_back(imageInfo);
    }

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = descriptorSet;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.descriptorCount = static_cast<uint32_t>(imageInfos.size());
    writeDescriptorSet.descriptorType
        = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;  // TODO: make this a param instead
    writeDescriptorSet.pImageInfo = imageInfos.data();

    vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, nullptr);
}

// UTILITY
NPFrame& Context::getCurrentFrame(uint32_t currentFrame)
{
    return frames[currentFrame];
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
    fn(instance, &createInfo, nullptr, &debugMessenger);
}

VKAPI_ATTR VkBool32 VKAPI_CALL Context::sDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    DBG_PRINT("validation error: %s\n", pCallbackData->pMessage);
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

    for (int i = 0; i < FRAME_COUNT; i++)
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

void Context::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto* context = static_cast<Context*>(glfwGetWindowUserPointer(window));
    context->framebufferResized = true;
}
