#pragma once

#include "structs.h"

#include <GLFW/glfw3.h>

#include <vector>
#include <unordered_map>
#include <atomic>

NP_TRACER_NAMESPACE_BEGIN

class Context
{
public:
    uint32_t kFramesInFlight;
    bool framebufferResized = false;
    std::atomic<uint32_t> frameIndex = 0u;

    // vulkan basics
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    Image depthImage;
    Image resultImage;
    Image accumulationImage;
    VkFormat depthFormat;
    VkDescriptorSet rtDescriptorSet;
    std::vector<Frame> frames;
    VkDeviceSize scratchAlignment;

    // swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    SwapchainParams swapchainParams;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkSemaphore> doneRenderingSemaphores;

    // queues
    std::unordered_map<QueueType, Queue> queues;
    std::vector<uint32_t> queueFamilyIndices;  // stored indices based on if they are supported
    VkCommandBuffer transferCommandBuffer = VK_NULL_HANDLE;

    void setFramesInFlight(const uint32_t count)
    {
        kFramesInFlight = count;
    }

    void createWindow(GLFWwindow*& window, uint32_t width, uint32_t height);
    void createInstance(bool enableDebug);
    void createPhysicalDevice();
    void createLogicalDeviceAndQueues();
    void createAllocator();
    void createSyncAndFrameObjects(size_t numRenderingSemaphores);

    // swapchain
    void createSurface(GLFWwindow* window);
    void createSwapchain(GLFWwindow* window);
    void recreateSwapchain(GLFWwindow* window);
    void cleanupSwapchain();

    // command buffers
    void createCommandBuffer(VkCommandBuffer& commandBuffer, QueueType queueFamily);
    void beginCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags = 0);
    void endCommandBuffer(VkCommandBuffer commandBuffer, QueueType queueFamily,
                          VkPipelineStageFlags waitDstFlags = 0, VkFence fence = VK_NULL_HANDLE,
                          VkSemaphore waitSemaphores = VK_NULL_HANDLE,
                          VkSemaphore signalSemaphores = VK_NULL_HANDLE);
    void freeCommandBuffer(VkCommandBuffer commandBuffer, QueueType queueFamily);

    // buffers
    bool createBuffer(Buffer& handle, VkDeviceSize size, VkBufferUsageFlags usage,
                      VmaAllocationCreateFlags allocationFlags) const;
    bool createDeviceLocalBuffer(Buffer& handle, const void* data, VkDeviceSize size,
                                 VkBufferUsageFlags usage);
    void copyBuffer(Buffer& src, Buffer& dst, VkDeviceSize size);
    VkDeviceAddress getBufferDeviceAddress(Buffer& buffer);

    // images
    void createImage(Image& handle, VkImageType type, VkFormat format, uint32_t width,
                     uint32_t height, VkImageUsageFlags usage,
                     VmaAllocationCreateFlags allocationFlags,
                     VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT,
                     bool shouldCreateView = true) const;
    void createTextureImage(Image& handle, void* pixels, uint32_t width, uint32_t height,
                            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
    void createDepthImage(uint32_t width, uint32_t height);
    void createResultImages(uint32_t width, uint32_t height);
    void createTextureSampler(VkSampler& sampler);  // pass as reference as it is still `nullptr` here
    void copyBufferToImage(VkCommandBuffer commandBuffer, Buffer& src, Image& dst, uint32_t width,
                           uint32_t height);
    void copyImageToBuffer(VkCommandBuffer commandBuffer, Image& src, Buffer& dst, uint32_t width,
                           uint32_t height,
                           VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);
    void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask,
                               VkPipelineStageFlags2 srcStageMask,
                               VkPipelineStageFlags2 dstStageMask,
                               VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

    // acceleration structures
    void createBottomLevelAccelerationStructure(VkCommandBuffer& commandBuffer,
                                                AccelerationStructure& handle,
                                                VkDeviceAddress vertexAddress,
                                                VkDeviceAddress indexAddress, uint32_t firstVertex,
                                                uint32_t vertexCount, uint32_t firstIndex,
                                                uint32_t indexCount);

    void createTopLevelAccelerationStructure(VkCommandBuffer& commandBuffer,
                                             AccelerationStructure& handle,
                                             Buffer& instanceBufferHandle,
                                             std::vector<FLOAT4x4>& transforms,
                                             std::vector<AccelerationStructure>& blasses);

    // descriptors
    void createDescriptorSetLayout(
        DescriptorSetLayout& descriptorSetLayout,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings,
        const std::vector<VkDescriptorBindingFlags>* pBindingFlags = nullptr,
        VkDescriptorSetLayoutCreateFlags layoutCreateFlags = 0,
        VkDescriptorPoolCreateFlags poolCreateFlags = 0) const;
    void allocateDescriptorSet(VkDescriptorSet& descriptorSet,
                               DescriptorSetLayout& descriptorSetLayout);
    void writeDescriptorSetBuffers(VkDescriptorSet& descriptorSet,
                                   std::vector<Buffer*>& bindingBuffers,
                                   std::vector<VkDescriptorSetLayoutBinding>& bindings);
    void writeDescriptorSetImages(
        const VkDescriptorSet& descriptorSet, uint32_t binding, const std::vector<Image>& images,
        VkSampler inSampler, VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;
    void writeDescriptorSetAccelerationStructures(
        const VkDescriptorSet& descriptorSet,
        const std::vector<AccelerationStructure*>& bindingAccelStructs,
        const std::vector<VkDescriptorSetLayoutBinding>& bindings) const;

    // utility
    Frame& getCurrentFrame(uint32_t currentFrame);
    void loadRayTracingFunctionPointers();

    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    void waitIdle();
    void destroyDebugMessenger();
    void destroy();

    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR
        = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;

private:
    // debug
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    void createDebugMessenger(bool enableDebug);

    static VKAPI_ATTR VkBool32 VKAPI_CALL sDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData);

    static void sPopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    static void sFramebufferResizeCallback(GLFWwindow* window, int width, int height);

    template<typename T>
    inline T sLoadDeviceFunction(VkDevice device, VkInstance instance, const char* name)
    {
        T fn = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));
        if (!fn)
        {
            fn = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));
        }
        return fn;
    }
};

NP_TRACER_NAMESPACE_END
